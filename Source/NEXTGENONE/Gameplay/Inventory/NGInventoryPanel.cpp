#include "NGInventoryPanel.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "NGPlayerInventoryManagerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

UNGInventoryPanel::UNGInventoryPanel(const FObjectInitializer& Init) : Super(Init)
{
	SlotFilter = TAG_NGSlotStorage;
}

void UNGInventoryPanel::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

void UNGInventoryPanel::SetInventorySource(UNGInventoryManagerComponent* Source)
{
	UnbindItems();
	if (Inventory.IsValid()) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::Changed);
	Inventory = Source;
	if (Source) Source->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::Changed);
	Refresh();
}

UNGPlayerInventoryManagerComponent* UNGInventoryPanel::ResolvePlayerInventory() const
{
	const auto* PC = GetOwningPlayer();
	const auto* Player = PC ? PC->GetPlayerState<APlayerState>() : nullptr;
	return Player ? Player->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
}

void UNGInventoryPanel::UnbindItems()
{
	for (auto Item : ObservedItems) if (Item.IsValid()) Item->OnStatsChanged.RemoveDynamic(this, &ThisClass::Refresh);
	ObservedItems.Reset();
}

void UNGInventoryPanel::NativeDestruct()
{
	UnbindItems();
	if (Inventory.IsValid()) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::Changed);
	Inventory.Reset();
	SetInventoryData(InventoryTitle, {}, nullptr);
	Super::NativeDestruct();
}

void UNGInventoryPanel::Changed() { Refresh(); }

void UNGInventoryPanel::Refresh()
{
	TArray<TWeakObjectPtr<UNGInventoryItemInstance>> CurrentItems;
	TArray<FUIInventorySlotData> Slots;
	if (Inventory.IsValid())
	{
		for (int32 Index = 0; Index < Inventory->GetSlotCount(); ++Index)
		{
			const FGameplayTag Tag = Inventory->GetSlotTag(Index);
			if (SlotFilter.IsValid() && !Tag.MatchesTag(SlotFilter)) continue;
			auto& SlotInfo = Slots.AddDefaulted_GetRef();
			SlotInfo.Inventory = Inventory.Get(); SlotInfo.SlotIndex = Index;
			auto* Item = Inventory->GetItemAtSlot(Index);
			if (!IsValid(Item) || !Item->GetItemDef()) continue;
			const auto* Def = GetDefault<UNGInventoryItemDefinition>(Item->GetItemDef());
			SlotInfo.Item = Item; SlotInfo.Name = Def->DisplayName; SlotInfo.Icon = Def->Icon;
			SlotInfo.Count = Inventory->GetCount(Item);
			SlotInfo.bEquipped = Item->IsEquipped();
			CurrentItems.Add(Item);
			if (!ObservedItems.Contains(Item)) Item->OnStatsChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
		}
	}
	for (auto Item : ObservedItems)
		if (Item.IsValid() && !CurrentItems.Contains(Item)) Item->OnStatsChanged.RemoveDynamic(this, &ThisClass::Refresh);
	ObservedItems = MoveTemp(CurrentItems);
	SetInventoryData(InventoryTitle, Slots, this);
}

bool UNGInventoryPanel::RequestInventoryOperation_Implementation(const FUIInventorySlotData& SlotInfo, EUIInventoryOperation Operation, int32 Amount)
{
	auto* Transport = ResolvePlayerInventory();
	auto* Item = Cast<UNGInventoryItemInstance>(SlotInfo.Item.Get());
	// UI snapshots are untrusted: resolve ownership/count again, then use existing server-validated requests.
	if (!Transport || Inventory.Get() != Transport
		|| SlotInfo.Inventory != Inventory.Get() || Inventory->GetItemAtSlot(SlotInfo.SlotIndex) != Item || !Inventory->Contains(Item) || Amount < 0 || Amount > Inventory->GetCount(Item)) return false;
	if (Operation == EUIInventoryOperation::SecondaryAction)
	{
		return Transport->RequestActivationItem(Item);
	}
	if (Operation == EUIInventoryOperation::Drop)
		return Transport->RequestDropItem(Item, Amount);
	return false;
}

bool UNGInventoryPanel::MoveInventoryItem_Implementation(const FUIInventorySlotData& Source, const FUIInventorySlotData& Destination, int32 Amount)
{
	auto* Transport = ResolvePlayerInventory();
	auto* SourceInventory = Cast<UNGInventoryManagerComponent>(Source.Inventory.Get());
	auto* TargetInventory = Cast<UNGInventoryManagerComponent>(Destination.Inventory.Get());
	auto* Item = Cast<UNGInventoryItemInstance>(Source.Item.Get());
	auto* ExpectedTarget = Cast<UNGInventoryItemInstance>(Destination.Item.Get());
	if (!Transport || !IsValid(SourceInventory) || !IsValid(TargetInventory) || TargetInventory != Inventory.Get()
		|| !IsValid(Item) || SourceInventory->GetItemAtSlot(Source.SlotIndex) != Item || Amount < 0 || Amount > SourceInventory->GetCount(Item)
		|| TargetInventory->GetItemAtSlot(Destination.SlotIndex) != ExpectedTarget) return false;
	return Transport->RequestInventoryTransfer(SourceInventory, Item, Source.SlotIndex, TargetInventory, Destination.SlotIndex, ExpectedTarget, Amount);
}
