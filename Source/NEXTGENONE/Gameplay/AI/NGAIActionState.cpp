#include "Gameplay/AI/NGAIActionState.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/NGAIEvents.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "GCFSMUtilities.h"
#include "TimerManager.h"
#include "Engine/World.h"

TArray<FName> UNGAIActionState::GetDeclaredTransitionEvents() const
{
	return {NGAIEvents::Succeeded, NGAIEvents::Failed, NGAIEvents::Aborted};
}

void UNGAIActionState::Enter()
{
	AbortAction(TEXT("Reentered"));
	++RunSerial;
	Super::Enter();
	ActionController = Context.Get();
	ExpectedPawn = ActionController.IsValid() ? ActionController->GetPawn() : nullptr;
	PossessionSerial = ActionController.IsValid() ? ActionController->GetPossessionSerial() : 0;
	ActionASC = ActionController.IsValid() ? ActionController->GetReadyAbilitySystem(ExpectedPawn.Get()) : nullptr;
	const auto* ProjectASC = Cast<UNGPlayerAbilitySystemComponent>(ActionASC.Get());
	AvatarGeneration = ProjectASC ? ProjectASC->GetAvatarGeneration() : 0;
	bRunning = true;
	Result = ENGAIActionResult::Running;
	ResultReason = NAME_None;
	if (!GetWorld() || !ProjectASC || !FMath::IsFinite(ActionTimeout) || ActionTimeout < .1f
		|| !ActionController->RegisterAction(this))
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Action not ready or another action owns execution")); return; }
	Deadline = GetWorld()->GetTimeSeconds() + ActionTimeout;
	GetWorld()->GetTimerManager().SetTimer(WatchdogTimer, this, &ThisClass::CheckAction, .1f, true);
	StartAction();
}

bool UNGAIActionState::HasCurrentAvatar() const
{
	const auto* ProjectASC = Cast<UNGPlayerAbilitySystemComponent>(ActionASC.Get());
	return ActionController.IsValid() && ExpectedPawn.IsValid() && ProjectASC
		&& ActionController->GetPawn() == ExpectedPawn.Get() && ProjectASC->GetAvatarActor() == ExpectedPawn.Get()
		&& ActionController->GetPossessionSerial() == PossessionSerial
		&& ProjectASC->GetAvatarGeneration() == AvatarGeneration;
}

bool UNGAIActionState::IsCurrentRun(uint64 Serial) const
{
	return bRunning && Serial == RunSerial && HasCurrentAvatar();
}

void UNGAIActionState::CheckAction()
{
	if (!bRunning) return;
	if (!HasCurrentAvatar() || ActionController->GetReadyAbilitySystem(ExpectedPawn.Get()) != ActionASC.Get())
	{ AbortAction(TEXT("Avatar or gameplay readiness lost")); return; }
	if (GetWorld()->GetTimeSeconds() >= Deadline) FinishAction(ENGAIActionResult::Failed, TEXT("Action timed out"));
}

void UNGAIActionState::FinishAction(ENGAIActionResult Outcome, FName Reason)
{
	if (!bRunning || Outcome == ENGAIActionResult::Running) return;
	bRunning = false; // Cleanup may synchronously fire path/ability callbacks.
	Result = Outcome;
	ResultReason = Reason;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(WatchdogTimer);
	ReleaseActionResources();
	if (auto* Controller = ActionController.Get()) Controller->UnregisterAction(this);
	UE_LOG(LogTemp, Verbose, TEXT("AI action %s: result=%d reason=%s"), *GetName(), static_cast<int32>(Outcome), *Reason.ToString());
	const FName Event = Outcome == ENGAIActionResult::Succeeded ? NGAIEvents::Succeeded
		: Outcome == ENGAIActionResult::Failed ? NGAIEvents::Failed : NGAIEvents::Aborted;
	if (!GetWorld() || Event.IsNone()) return;
	const uint64 CompletedRun = RunSerial;
	// Defer graph notification until Enter/activation/callback stacks have unwound.
	CompletionTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, CompletedRun, Event]()
	{
		if (CompletedRun != RunSerial || bRunning || !HasCurrentAvatar()) return;
		if (auto* FSM = GetParentFSM(); FSM && FSM->GetActiveState() == this)
			UGCFSMUtilities::TriggerEvent(ActionController.Get(), Event, EGCFSMTriggerEventTargetPolicy::ContextObject, true, false,
				EGCFSMTriggerEventQueuePolicy::QueueUnlessAlreadyInQueue, .1f);
	}));
}

void UNGAIActionState::AbortAction(FName Reason)
{
	FinishAction(ENGAIActionResult::Aborted, Reason);
}

void UNGAIActionState::Exit(FName Event)
{
	++RunSerial;
	AbortAction(TEXT("State exited"));
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CompletionTimer);
	ActionController.Reset(); ExpectedPawn.Reset(); ActionASC.Reset();
	Super::Exit(Event);
}
