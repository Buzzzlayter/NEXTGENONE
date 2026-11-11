#include "Gameplay/AI/NGAIController.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/AI/NGAIBehavior.h"
#include "Gameplay/AI/NGBotCharacter.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"

ANGAIController::ANGAIController()
{
	// Character initialization requires the GameMode's ANextgenonePlayerState (ASC + inventory).
	bWantsPlayerState = true;
	// A future brain must wait for the current Avatar and Character FSM resources.
	bStartAILogicOnPossess = false;
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
	SightConfig->SightRadius = 2500.f;
	SightConfig->LoseSightRadius = 3000.f;
	SightConfig->PeripheralVisionAngleDegrees = 80.f;
	SightConfig->SetMaxAge(5.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	// Memory applies the shared policy; sensing all affiliations also supports changing teams.
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
	HearingConfig->HearingRange = 2000.f;
	HearingConfig->SetMaxAge(5.f);
	HearingConfig->DetectionByAffiliation = SightConfig->DetectionByAffiliation;
	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->ConfigureSense(*HearingConfig);
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	Memory = CreateDefaultSubobject<UNGAIMemoryComponent>(TEXT("Memory"));
}

UNGAbilitySystemComponent* ANGAIController::GetReadyAbilitySystem(const APawn* ExpectedPawn) const
{
	APawn* ControlledPawn = GetPawn();
	if (!HasActorBegunPlay() || IsActorBeingDestroyed() || !HasAuthority() || !IsValid(ControlledPawn) || ControlledPawn != ExpectedPawn
		|| ControlledPawn->GetController() != this) return nullptr;
	const auto* Machine = ObservedCharacterFSM.Get();
	if (!Machine || Machine->GetOwner() != ControlledPawn || !Machine->IsGameplayReady(TAG_NGAlive)) return nullptr;
	auto* ASC = Cast<UNGAbilitySystemComponent>(Machine->GetReadyASC());
	return ASC && ASC->GetOwnerActor() == GetPlayerState<APlayerState>() ? ASC : nullptr;
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
	if (!HasAuthority() || !HasActorBegunPlay() || IsActorBeingDestroyed()) return;
	ReleasePawn();
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;
	const auto* Bot = Cast<ANGBotCharacter>(ControlledPawn);
	const TSubclassOf<UNGAIBehavior> SelectedBehavior = Bot && Bot->AIBehaviorClass
		? Bot->AIBehaviorClass : BehaviorClass;
	if (!SelectedBehavior) Behavior = nullptr;
	else if (!Behavior || Behavior->GetClass() != SelectedBehavior.Get())
	{
		Behavior = NewObject<UNGAIBehavior>(this, SelectedBehavior);
		Behavior->Initialize(this);
	}
	ObservedCharacterFSM = ControlledPawn->FindComponentByClass<UNGCharacterStateMachineComponent>();
	if (auto* Machine = ObservedCharacterFSM.Get())
	{
		ResourcesReadyHandle = Machine->OnResourcesReady.AddUObject(this, &ThisClass::TryInitializeGameplay);
		// The Pawn may have completed initialization before possession.
		TryInitializeGameplay(ControlledPawn);
	}
}

void ANGAIController::TryInitializeGameplay(AActor* Avatar)
{
	if (Avatar != GetPawn() || !ResourcesReadyHandle.IsValid() || !GetReadyAbilitySystem(GetPawn())) return;
	InitializeGameplay();
}

void ANGAIController::InitializeGameplay()
{
	auto* Machine = ObservedCharacterFSM.Get();
	Machine->OnResourcesReady.Remove(ResourcesReadyHandle);
	ResourcesReadyHandle.Reset();
	// Runtime subscriptions are installed once, only after initialization completes.
	StateChangedHandle = Machine->OnStateChanged.AddUObject(this, &ThisClass::HandleGameplayStateChanged);
	ResourceFailureHandle = Machine->OnResourceFailure.AddUObject(this, &ThisClass::StopBehavior);
	Memory->SetEnabled(true);
	if (Behavior) Behavior->Start();
}

void ANGAIController::HandleGameplayStateChanged(FGameplayTag Previous, FGameplayTag Current)
{
	if (!Current.MatchesTag(TAG_NGAlive)) StopBehavior();
}

void ANGAIController::UnsubscribeGameplay()
{
	if (auto* Machine = ObservedCharacterFSM.Get())
	{
		Machine->OnStateChanged.Remove(StateChangedHandle);
		Machine->OnResourcesReady.Remove(ResourcesReadyHandle);
		Machine->OnResourceFailure.Remove(ResourceFailureHandle);
	}
	StateChangedHandle.Reset(); ResourcesReadyHandle.Reset(); ResourceFailureHandle.Reset();
}

void ANGAIController::ReleasePawn()
{
	// Preserve whether Start was delivered, then detach before any BP cleanup callbacks.
	const FDelegateHandle InitializedSubscription = StateChangedHandle;
	UnsubscribeGameplay();
	ObservedCharacterFSM.Reset();
	Memory->SetEnabled(false);
	if (InitializedSubscription.IsValid() && Behavior) Behavior->Stop();
}

void ANGAIController::StopBehavior()
{
	if (!StateChangedHandle.IsValid()) return;
	UnsubscribeGameplay();
	Memory->SetEnabled(false);
	if (Behavior) Behavior->Stop();
}

void ANGAIController::RefreshObservation()
{
	const auto* Target = bObserveTarget ? Memory->GetTarget() : nullptr;
	if (Target)
	{
		if (Target->bVisible) SetFocus(Target->Actor.Get(), EAIFocusPriority::Gameplay);
		else SetFocalPoint(Target->GetKnownLocation(), EAIFocusPriority::Gameplay);
		bOwnsObservationFocus = true;
	}
	else if (bOwnsObservationFocus)
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		bOwnsObservationFocus = false;
	}
}

void ANGAIController::OnUnPossess()
{
	ReleasePawn();
	Super::OnUnPossess();
}

void ANGAIController::EndPlay(EEndPlayReason::Type Reason)
{
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
