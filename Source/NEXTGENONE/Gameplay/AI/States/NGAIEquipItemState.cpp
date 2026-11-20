#include "Gameplay/AI/States/NGAIEquipItemState.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Gameplay/Inventory/NGPlayerInventoryManagerComponent.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryItemInstance.h"

void UNGAIEquipItemState::StartAction()
{
	const auto* Owner = ActionASC->GetOwnerActor();
	auto* Inventory = Owner ? Owner->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
	const auto* ItemDefinition = ItemClass.GetDefaultObject();
	const auto* Equipment = ItemDefinition ? Cast<UNGEquipmentDefinition>(ItemDefinition->GetEquipmentDefinition()) : nullptr;
	if (!Inventory || !Equipment)
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("Inventory or equipment definition is missing"));
		return;
	}

	const int32 EquipmentSlot = Inventory->FindSlotByTag(Equipment->SlotTag, 0);
	if (EquipmentSlot == INDEX_NONE)
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("Equipment slot is missing"));
		return;
	}

	auto* SlottedItem = Inventory->GetItemAtSlot(EquipmentSlot);
	auto* Item = SlottedItem && SlottedItem->GetItemDef().Get() == ItemClass.Get() ? SlottedItem : nullptr;
	for (int32 Index = 0; !Item && Index < Inventory->GetSlotCount(); ++Index)
	{
		auto* Candidate = Inventory->GetItemAtSlot(Index);
		if (Candidate && Candidate->GetItemDef().Get() == ItemClass.Get()) Item = Candidate;
	}

	if (!Item)
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("Requested item is not in the inventory"));
		return;
	}
	if (Item->IsEquipped())
	{
		FinishAction(ENGAIActionResult::Succeeded, TEXT("Item already equipped"));
		return;
	}

	PendingItem = Item;
	Item->OnStatsChanged.AddUniqueDynamic(this, &ThisClass::HandleItemChanged);
	if (Item != SlottedItem && !Inventory->ActivateItemOnServer(Item))
		FinishAction(ENGAIActionResult::Failed, TEXT("Item activation rejected"));
}

void UNGAIEquipItemState::HandleItemChanged()
{
	const auto* Item = PendingItem.Get();
	if (Item && Item->IsEquipped() && IsCurrentRun(RunSerial))
		FinishAction(ENGAIActionResult::Succeeded, TEXT("Item equipped"));
}

void UNGAIEquipItemState::ReleaseActionResources()
{
	if (auto* Item = PendingItem.Get())
		Item->OnStatsChanged.RemoveDynamic(this, &ThisClass::HandleItemChanged);
	PendingItem.Reset();
}
