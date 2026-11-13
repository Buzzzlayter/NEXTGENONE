#include "Gameplay/StateMachine/NGPlayerLifecycleComponent.h"

#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HFSMLibrary.h"
#include "HFSMState.h"

void UNGPlayerLifecycleComponent::Enter()
{
	Unbind();
	Super::Enter();
	auto* PC = GetPlayerController();
	if (!IsValid(PC) || !PC->IsLocalController()) return;
	Player = PC;
	PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::PawnChanged);
	BindPawn(PC->GetPawn());
}

void UNGPlayerLifecycleComponent::BindPawn(APawn* Pawn)
{
	if (auto* Machine = CharacterMachine.Get()) Machine->OnStateChanged.Remove(StateHandle);
	StateHandle.Reset();
	CharacterMachine.Reset();
	ObservedPawn = Pawn;
	if (auto* Machine = Pawn ? Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>() : nullptr)
	{
		CharacterMachine = Machine;
		StateHandle = Machine->OnStateChanged.AddUObject(this, &ThisClass::StateChanged);
		if (Machine->HasStateTag(TAG_NGDead)) StateChanged({}, TAG_NGDead);
	}
}

void UNGPlayerLifecycleComponent::PawnChanged(APawn* PreviousPawn, APawn* NewPawn)
{
	const auto* PC = Player.Get();
	if (!PC || PC->GetPawn() != NewPawn || ObservedPawn.Get() == NewPawn) return;
	// New possession is not readiness. The UI re-enters its existing WaitForReady gate.
	UHFSMLibrary::QueueNamedEvent(GetState(), PawnChangedEvent);
	BindPawn(NewPawn);
}

void UNGPlayerLifecycleComponent::StateChanged(FGameplayTag Previous, FGameplayTag Current)
{
	const auto* PC = Player.Get();
	if (!PC || PC->GetPawn() != ObservedPawn.Get() || !CharacterMachine.IsValid()) return;
	if (Current.MatchesTag(TAG_NGDead) && !Previous.MatchesTag(TAG_NGDead))
		UHFSMLibrary::QueueNamedEvent(GetState(), DeathEvent);
}

void UNGPlayerLifecycleComponent::Unbind()
{
	if (auto* PC = Player.Get()) PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::PawnChanged);
	if (auto* Machine = CharacterMachine.Get()) Machine->OnStateChanged.Remove(StateHandle);
	StateHandle.Reset();
	CharacterMachine.Reset();
	ObservedPawn.Reset();
	Player.Reset();
}

void UNGPlayerLifecycleComponent::Exit(FName Event)
{
	Unbind();
	Super::Exit(Event);
}

void UNGPlayerLifecycleComponent::BeginDestroy()
{
	Unbind();
	Super::BeginDestroy();
}
