#include "NGInteractionStateComponent.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HFSMLibrary.h"
#include "HFSMState.h"

UNGInteractionStateComponent::UNGInteractionStateComponent() { bIsTickEnabled = true; }

void UNGInteractionStateComponent::Enter()
{
	if (auto* PC = ObservedController.Get()) PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::PawnChanged);
	ObservedController.Reset();
	BindPawn(nullptr);
	bActive = true;
	bClosedSent = false;
	Super::Enter();
	if (auto* PC = GetPlayerController())
	{
		ObservedController = PC;
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::PawnChanged);
		BindPawn(PC->GetPawn());
	}
	Route(); // Subscribe before reading: already-confirmed interactions must also work.
}

void UNGInteractionStateComponent::BindPawn(APawn* NewPawn)
{
	if (ObservedInteraction.IsValid()) ObservedInteraction->OnContextChanged.RemoveAll(this);
	ObservedInteraction = NewPawn ? NewPawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	if (ObservedInteraction.IsValid()) ObservedInteraction->OnContextChanged.AddUObject(this, &ThisClass::Route);
}

void UNGInteractionStateComponent::PawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindPawn(NewPawn);
	Route();
}

void UNGInteractionStateComponent::Route()
{
	if (!bActive) return;
	const auto* Interaction = ObservedInteraction.Get();
	const auto* PC = GetPlayerController();
	const APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	const auto* Machine = PlayerPawn ? PlayerPawn->FindComponentByClass<UNGCharacterStateMachineComponent>() : nullptr;
	if (!Interaction || Interaction->GetOwner() != PlayerPawn || !Interaction->GetContext().IsValid()
		|| (Machine && (Machine->HasStateTag(TAG_NGDead) || Machine->HasResourceFailure())))
	{
		if (!bClosedSent && !ClosedEvent.IsNone()) UHFSMLibrary::QueueNamedEvent(GetState(), ClosedEvent);
		bClosedSent = true;
		return;
	}
	bClosedSent = false;
	if (const auto* Event = Routes.Find(Interaction->GetContext().Type); Event && !Event->IsNone())
		UHFSMLibrary::QueueNamedEvent(GetState(), *Event);
}

void UNGInteractionStateComponent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Only detect context loss here; positive routing is event-driven, never repeated each frame.
	if (bActive && !bClosedSent)
	{
		const auto* Interaction = ObservedInteraction.Get();
		const auto* PC = GetPlayerController();
		const APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
		const auto* Machine = PlayerPawn ? PlayerPawn->FindComponentByClass<UNGCharacterStateMachineComponent>() : nullptr;
		if (!Interaction || Interaction->GetOwner() != PlayerPawn || !Interaction->GetContext().IsValid()
			|| (Machine && (Machine->HasStateTag(TAG_NGDead) || Machine->HasResourceFailure()))) Route();
	}
}

void UNGInteractionStateComponent::Exit(FName Event)
{
	bActive = false;
	if (auto* PC = ObservedController.Get()) PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::PawnChanged);
	ObservedController.Reset();
	if (ObservedInteraction.IsValid()) ObservedInteraction->OnContextChanged.RemoveAll(this);
	ObservedInteraction.Reset();
	Super::Exit(Event);
}

void UNGInteractionSessionStateComponent::Enter()
{
	auto* PC = GetPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	SessionInteraction = PlayerPawn ? PlayerPawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	Super::Enter();
}

void UNGInteractionSessionStateComponent::Exit(FName Event)
{
	Super::Exit(Event); // Stop observing before termination broadcasts a context change.
	if (auto* Interaction = SessionInteraction.Get()) Interaction->EndInteraction();
	SessionInteraction.Reset();
}
