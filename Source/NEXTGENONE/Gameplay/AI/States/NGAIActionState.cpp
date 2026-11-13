#include "Gameplay/AI/States/NGAIActionState.h"

#include "Gameplay/AI/NGAIController.h"
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
	ResetAction();
	Super::Enter();
	ActionController = Context;
	ExpectedPawn = ActionController.IsValid() ? ActionController->GetPawn() : nullptr;
	ActionASC = ActionController.IsValid() ? ActionController->GetReadyAbilitySystem(ExpectedPawn.Get()) : nullptr;
	const auto* ProjectASC = Cast<UNGPlayerAbilitySystemComponent>(ActionASC.Get());
	AvatarGeneration = ProjectASC ? ProjectASC->GetAvatarGeneration() : 0;
	bRunning = true;
	Result = ENGAIActionResult::Running;
	ResultReason = NAME_None;
	if (!GetWorld() || !ProjectASC || !FMath::IsFinite(ActionTimeout) || ActionTimeout < .1f)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Action context or gameplay not ready")); return; }
	const uint64 StartedRun = RunSerial;
	GetWorld()->GetTimerManager().SetTimer(TimeoutTimer, FTimerDelegate::CreateWeakLambda(this, [this, StartedRun]()
	{
		if (StartedRun != RunSerial || !bRunning) return;
		if (!IsCurrentRun(StartedRun)) { ResetAction(); return; }
		FinishAction(ENGAIActionResult::Failed, TEXT("Action timed out"));
	}), ActionTimeout, false);
	StartAction();
}

bool UNGAIActionState::HasCurrentAvatar() const
{
	const auto* ProjectASC = Cast<UNGPlayerAbilitySystemComponent>(ActionASC.Get());
	return ActionController.IsValid() && ExpectedPawn.IsValid() && ProjectASC
		&& ActionController->GetPawn() == ExpectedPawn.Get() && ProjectASC->GetAvatarActor() == ExpectedPawn.Get()
		&& ExpectedPawn->GetController() == ActionController.Get()
		&& ProjectASC->GetAvatarGeneration() == AvatarGeneration;
}

bool UNGAIActionState::IsCurrentRun(uint64 Serial) const
{
	return bRunning && Serial == RunSerial && HasCurrentAvatar()
		&& GetParentFSM() && GetParentFSM()->GetActiveState() == this;
}

void UNGAIActionState::FinishAction(ENGAIActionResult Outcome, FName Reason)
{
	if (!bRunning || Outcome == ENGAIActionResult::Running) return;
	bRunning = false; // Cleanup may synchronously fire path/ability callbacks.
	Result = Outcome;
	ResultReason = Reason;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TimeoutTimer);
	ReleaseActionResources();
	UE_LOG(LogTemp, Verbose, TEXT("AI action %s: result=%d reason=%s"), *GetName(), static_cast<int32>(Outcome), *Reason.ToString());
	const FName Event = Outcome == ENGAIActionResult::Succeeded ? NGAIEvents::Succeeded : NGAIEvents::Failed;
	if (!GetWorld() || Event.IsNone()) return;
	const uint64 CompletedRun = RunSerial;
	// Defer graph notification until Enter/activation/callback stacks have unwound.
	CompletionTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, CompletedRun, Event]()
	{
		if (CompletedRun != RunSerial || bRunning || !HasCurrentAvatar()
			|| !GetParentFSM() || GetParentFSM()->GetActiveState() != this) return;
		TriggerTransitionEvent(Event);
	}));
}

void UNGAIActionState::ResetAction()
{
	++RunSerial;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimeoutTimer);
		GetWorld()->GetTimerManager().ClearTimer(CompletionTimer);
	}
	if (!bRunning) return;
	bRunning = false;
	Result = ENGAIActionResult::Aborted;
	ResultReason = TEXT("State exited or execution invalidated");
	ReleaseActionResources();
}

void UNGAIActionState::Exit(FName Event)
{
	ResetAction();
	ActionController.Reset(); ExpectedPawn.Reset(); ActionASC.Reset();
	Super::Exit(Event);
}
