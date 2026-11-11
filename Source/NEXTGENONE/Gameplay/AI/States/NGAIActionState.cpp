#include "Gameplay/AI/States/NGAIActionState.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/NGAIBehavior.h"
#include "Gameplay/AI/NGAIEvents.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

TArray<FName> UNGAIActionState::GetDeclaredTransitionEvents() const
{
	return {NGAIEvents::Succeeded, NGAIEvents::Failed};
}

void UNGAIActionState::Enter()
{
	AbortAction(TEXT("Reentered"));
	++RunSerial;
	Super::Enter();
	ActionController = Context ? Context->GetController() : nullptr;
	ExpectedPawn = ActionController.IsValid() ? ActionController->GetPawn() : nullptr;
	ActionASC = ActionController.IsValid() ? ActionController->GetReadyAbilitySystem(ExpectedPawn.Get()) : nullptr;
	const auto* ProjectASC = Cast<UNGPlayerAbilitySystemComponent>(ActionASC.Get());
	AvatarGeneration = ProjectASC ? ProjectASC->GetAvatarGeneration() : 0;
	bRunning = true;
	Result = ENGAIActionResult::Running;
	ResultReason = NAME_None;
	if (!GetWorld() || !ProjectASC || !FMath::IsFinite(ActionTimeout) || ActionTimeout < .1f
		|| !Context || ActionController->GetBehavior() != Context.Get())
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Action context or gameplay not ready")); return; }
	Deadline = GetWorld()->GetTimeSeconds() + ActionTimeout;
	GetWorld()->GetTimerManager().SetTimer(WatchdogTimer, this, &ThisClass::CheckAction, .1f, true);
	StartAction();
}

bool UNGAIActionState::HasCurrentAvatar() const
{
	const auto* ProjectASC = Cast<UNGPlayerAbilitySystemComponent>(ActionASC.Get());
	return ActionController.IsValid() && ExpectedPawn.IsValid() && ProjectASC
		&& Context && ActionController->GetBehavior() == Context.Get()
		&& ActionController->GetPawn() == ExpectedPawn.Get() && ProjectASC->GetAvatarActor() == ExpectedPawn.Get()
		&& ExpectedPawn->GetController() == ActionController.Get()
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
	UE_LOG(LogTemp, Verbose, TEXT("AI action %s: result=%d reason=%s"), *GetName(), static_cast<int32>(Outcome), *Reason.ToString());
	const FName Event = Outcome == ENGAIActionResult::Succeeded ? NGAIEvents::Succeeded : NGAIEvents::Failed;
	if (!GetWorld() || Event.IsNone()) return;
	const uint64 CompletedRun = RunSerial;
	// Defer graph notification until Enter/activation/callback stacks have unwound.
	CompletionTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, CompletedRun, Event]()
	{
		if (CompletedRun != RunSerial || bRunning || !HasCurrentAvatar()) return;
		TriggerTransitionEvent(Event);
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
