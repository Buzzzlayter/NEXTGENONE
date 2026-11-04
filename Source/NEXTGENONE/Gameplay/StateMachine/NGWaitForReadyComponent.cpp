#include "NGWaitForReadyComponent.h"
#include "NGCharacterStateMachineComponent.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "HFSMLibrary.h"
#include "HFSMState.h"

UNGWaitForReadyComponent::UNGWaitForReadyComponent()
{
	RequiredCharacterState = TAG_NGAlive;
}

void UNGWaitForReadyComponent::Enter()
{
	bWaiting = true;
	WaitingFor = TEXT("PlayerController");
	Super::Enter();
}

void UNGWaitForReadyComponent::Exit(FName Event)
{
	bWaiting = false;
	WaitingFor = NAME_None;
	Super::Exit(Event);
}

FName UNGWaitForReadyComponent::FindMissingDependency() const
{
	const auto* PC = GetPlayerController();
	if (!IsValid(PC) || !PC->IsLocalController() || !PC->GetLocalPlayer()) return TEXT("PlayerController");
	const auto* Player = PC->GetPlayerState<APlayerState>();
	if (!IsValid(Player)) return TEXT("PlayerState");
	const APawn* PlayerPawn = PC->GetPawn();
	if (!IsValid(PlayerPawn) || !PlayerPawn->HasActorBegunPlay() || PlayerPawn->GetController() != PC
		|| PlayerPawn->GetPlayerState() != Player) return TEXT("Pawn");
	const auto* Machine = PlayerPawn->FindComponentByClass<UNGCharacterStateMachineComponent>();
	if (!Machine || !Machine->IsGameplayReady(RequiredCharacterState)) return TEXT("CharacterState");
	const auto* ASC = Machine->GetReadyASC();
	if (!ASC || ASC->GetOwnerActor() != Player || ASC->GetAvatarActor() != PlayerPawn
		|| UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player) != ASC) return TEXT("AbilitySystem");
	const auto* Health = ASC->GetSet<UNGHealthSet>();
	if (!Health || Health->MaxHealth.GetCurrentValue() <= 0.f || Health->Health.GetCurrentValue() <= 0.f
		|| ASC->HasMatchingGameplayTag(TAG_NGDead)) return TEXT("Health");
	const auto* Inventory = Player->FindComponentByClass<UNGInventoryManagerComponent>();
	if (!Inventory || !Inventory->HasResolvedItems()) return TEXT("Inventory");
	return NAME_None;
}

void UNGWaitForReadyComponent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bWaiting) return;
	WaitingFor = FindMissingDependency();
	if (!WaitingFor.IsNone()) return;
	if (ReadyEvent.IsNone()) { WaitingFor = TEXT("Ready"); return; }
	// Queue on a subsequent state-machine update, never transition inside Components.Enter.
	bWaiting = false;
	UHFSMLibrary::QueueNamedEvent(GetState(), ReadyEvent);
}
