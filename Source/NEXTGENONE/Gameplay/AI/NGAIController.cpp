#include "Gameplay/AI/NGAIController.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/AI/NGAIActionState.h"
#include "Gameplay/AI/NGAIEvents.h"
#include "GameplayTags/NGGameplayTags.h"
#include "AbilitySystemGlobals.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GCFSMUtilities.h"
#include "TimerManager.h"

ANGAIController::ANGAIController()
{
	// Character initialization requires the GameMode's ANextgenonePlayerState (ASC + inventory).
	bWantsPlayerState = true;
	// A future brain must wait for the current Avatar and Character FSM resources.
	bStartAILogicOnPossess = false;
}

UNGAbilitySystemComponent* ANGAIController::GetReadyAbilitySystem(const APawn* ExpectedPawn) const
{
	APawn* ControlledPawn = GetPawn();
	if (bStopping || !HasAuthority() || !IsValid(ControlledPawn) || ControlledPawn != ExpectedPawn
		|| ControlledPawn->GetController() != this) return nullptr;
	const auto* Machine = ControlledPawn->FindComponentByClass<UNGCharacterStateMachineComponent>();
	if (!Machine || !Machine->IsGameplayReady(TAG_NGAlive)) return nullptr;
	auto* ASC = Cast<UNGAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn));
	return ASC && ASC->GetAvatarActor() == ControlledPawn && ASC->GetOwnerActor() == GetPlayerState<APlayerState>() ? ASC : nullptr;
}

void ANGAIController::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && GetPawn()) BindPawn();
}

void ANGAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (HasActorBegunPlay()) BindPawn();
}

void ANGAIController::BindPawn()
{
	if (!HasAuthority() || bStopping) return;
	ReleasePawn();
	if (APawn* ControlledPawn = GetPawn())
	{
		ObservedCharacterFSM = ControlledPawn->FindComponentByClass<UNGCharacterStateMachineComponent>();
		if (auto* Machine = ObservedCharacterFSM.Get())
		{
			StateChangedHandle = Machine->OnStateChanged.AddWeakLambda(this, [this](FGameplayTag, FGameplayTag) { RefreshGameplay(); });
			ResourcesReadyHandle = Machine->OnResourcesReady.AddWeakLambda(this, [this](AActor*) { RefreshGameplay(); });
		}
	}
	if (!BehaviorMachineName.IsNone())
	{
		const bool bWasRunning = UGCFSMUtilities::IsFSMRunning(this, BehaviorMachineName);
		if (!bWasRunning)
			UGCFSMUtilities::LaunchFSM(this, BehaviorMachineName, EGCFSMReplicationOptions::NonReplicated);
		UGCFSMUtilities::PauseFSMs(this, false);
		if (bWasRunning) SendBehaviorEvent(NotReadyEvent);
	}
	// Covers readiness changes without a state-tag transition (ASC handoff/resource acquisition).
	GetWorldTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::RefreshGameplay, .1f, true);
	RefreshGameplay();
}

void ANGAIController::RefreshGameplay()
{
	const bool bReadyNow = !bWaitingForGraphReset && GetReadyAbilitySystem(GetPawn()) != nullptr;
	if (bReadyNow == bGameplayReady) return;
	bGameplayReady = bReadyNow;
	if (!bReadyNow)
		if (auto* Action = ActiveAction.Get()) Action->AbortAction(TEXT("Gameplay unavailable"));
	SendBehaviorEvent(bReadyNow ? NGAIEvents::Ready : NotReadyEvent);
}

void ANGAIController::SendBehaviorEvent(FName Event)
{
	if (!BehaviorMachineName.IsNone() && !Event.IsNone() && UGCFSMUtilities::IsFSMRunning(this, BehaviorMachineName))
		UGCFSMUtilities::TriggerEvent(this, Event, EGCFSMTriggerEventTargetPolicy::ContextObject, true, false,
			EGCFSMTriggerEventQueuePolicy::DiscardQueuedEventsThenQueue);
}

bool ANGAIController::RegisterAction(UNGAIActionState* Action)
{
	if (!Action || bStopping || bWaitingForGraphReset || !GetReadyAbilitySystem(GetPawn()) || ActiveAction.IsValid()) return false;
	ActiveAction = Action;
	return true;
}

void ANGAIController::UnregisterAction(UNGAIActionState* Action)
{
	if (ActiveAction.Get() == Action) ActiveAction.Reset();
}

void ANGAIController::ReleasePawn()
{
	bWaitingForGraphReset = true;
	GetWorldTimerManager().ClearTimer(ReadinessTimer);
	if (auto* Action = ActiveAction.Get()) Action->AbortAction(TEXT("Pawn released"));
	++PossessionSerial;
	ActiveAction.Reset();
	if (auto* Machine = ObservedCharacterFSM.Get())
	{
		Machine->OnStateChanged.Remove(StateChangedHandle);
		Machine->OnResourcesReady.Remove(ResourcesReadyHandle);
	}
	ObservedCharacterFSM.Reset();
	StateChangedHandle.Reset(); ResourcesReadyHandle.Reset();
	bGameplayReady = false;
	SendBehaviorEvent(NotReadyEvent);
	if (!BehaviorMachineName.IsNone() && UGCFSMUtilities::IsFSMRunning(this, BehaviorMachineName))
		UGCFSMUtilities::PauseFSMs(this);
}

void ANGAIController::EnterGameplayWait()
{
	if (bStopping || !HasAuthority()) return;
	bWaitingForGraphReset = false;
	bGameplayReady = false;
	RefreshGameplay();
}

void ANGAIController::OnUnPossess()
{
	ReleasePawn();
	Super::OnUnPossess();
}

void ANGAIController::EndPlay(EEndPlayReason::Type Reason)
{
	bStopping = true;
	ReleasePawn();
	Super::EndPlay(Reason);
}

FVector ANGAIController::GetFocalPointOnActor(const AActor* Actor) const
{
	// Preserve ALS focus at the Pawn's view location without its Behavior Tree startup.
	if (const APawn* FocusPawn = Cast<APawn>(Actor))
	{
		return FocusPawn->GetPawnViewLocation();
	}
	return Super::GetFocalPointOnActor(Actor);
}
