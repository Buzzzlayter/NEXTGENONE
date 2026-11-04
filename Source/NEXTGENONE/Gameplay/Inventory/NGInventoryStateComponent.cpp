#include "NGInventoryStateComponent.h"
#include "NGInventoryPanel.h"
#include "NGPlayerInventoryManagerComponent.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

namespace
{
	UNGPlayerInventoryManagerComponent* ResolveInventory(APlayerController* PC)
	{
		auto* Player = PC ? PC->GetPlayerState<APlayerState>() : nullptr;
		return Player ? Player->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
	}
}

void UNGInventoryStateComponent::Enter()
{
	Super::Enter();
	auto* Panel = Cast<UNGInventoryPanel>(GetWidget());
	if (!Panel) return;
	auto* PC = GetPlayerController();
	auto* PlayerInventory = ResolveInventory(PC);
	UNGInventoryManagerComponent* InventorySource = PlayerInventory;
	if (Source == ENGInventoryPanelSource::Interaction)
	{
		APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
		auto* Interaction = PlayerPawn ? PlayerPawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
		InventorySource = Interaction && Interaction->GetContext().IsValid()
			? Cast<UNGInventoryManagerComponent>(Interaction->GetContext().Source.Get()) : nullptr;
		if (!PlayerInventory || PlayerInventory->GetViewedInventory() != InventorySource) InventorySource = nullptr;
	}
	Panel->SetInventorySource(InventorySource);
}

void UNGInventoryStateComponent::Exit(FName Event)
{
	if (auto* Panel = Cast<UNGInventoryPanel>(GetWidget())) Panel->SetInventorySource(nullptr);
	Super::Exit(Event);
}
