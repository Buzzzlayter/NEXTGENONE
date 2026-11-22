#include "NGWaitForReadyComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "NGCharacterStateMachineComponent.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "HFSMLibrary.h"
#include "HFSMState.h"
#include "LoadingProcessTask.h"

UNGWaitForReadyComponent::UNGWaitForReadyComponent()
{
	RequiredCharacterState = TAG_NGAlive;
}

void UNGWaitForReadyComponent::Enter()
{
	bWaiting = true;
	WaitingFor = TEXT("PlayerController");
	TryCreateLoadingTask();
	Super::Enter();
}

void UNGWaitForReadyComponent::Exit(FName Event)
{
	bWaiting = false;
	WaitingFor = NAME_None;
	if (LoadingTask)
	{
		LoadingTask->Unregister();
		LoadingTask = nullptr;
	}
	Super::Exit(Event);
}

void UNGWaitForReadyComponent::TryCreateLoadingTask()
{
	if (!LoadingTask && GetLocalPlayer())
	{
		LoadingTask = ULoadingProcessTask::CreateLoadingScreenProcessTask(this, TEXT("Waiting for the player UI to be ready."));
	}
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
	if (ASC->GetOwnerActor() != Player) return TEXT("AbilitySystem");
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
	TryCreateLoadingTask();
	WaitingFor = FindMissingDependency();
	if (!WaitingFor.IsNone()) return;
	if (ReadyEvent.IsNone()) { WaitingFor = TEXT("Ready"); return; }
	// Queue on a subsequent state-machine update, never transition inside Components.Enter.
	bWaiting = false;
	UHFSMLibrary::QueueNamedEvent(GetState(), ReadyEvent);
}
