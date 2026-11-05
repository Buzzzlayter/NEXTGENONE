#include "NGInventoryPanel.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Gameplay/Combat/NGCombat.h"
#include "NGPlayerInventoryManagerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

UNGInventoryPanel::UNGInventoryPanel(const FObjectInitializer& Init) : Super(Init)
{
	SlotFilter = TAG_NGSlotStorage;
}

void UNGInventoryPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (auto* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::PawnChanged);
		PawnChanged(nullptr, PC->GetPawn());
	}
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

void UNGInventoryPanel::PawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (Combat.IsValid()) Combat->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::EquipmentChanged);
	Combat = NewPawn ? NewPawn->FindComponentByClass<UNGCombatComponent>() : nullptr;
	if (Combat.IsValid()) Combat->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::EquipmentChanged);
	const auto* PC = GetOwningPlayer();
	const auto* Player = PC ? PC->GetPlayerState<APlayerState>() : nullptr;
	PlayerInventory = Player ? Player->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
	Refresh();
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
	if (Combat.IsValid()) Combat->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::EquipmentChanged);
	if (auto* PC = GetOwningPlayer()) PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::PawnChanged);
	Inventory.Reset(); Combat.Reset(); PlayerInventory.Reset();
	SetInventoryData(InventoryTitle, {}, nullptr);
	Super::NativeDestruct();
}

void UNGInventoryPanel::Changed(FNGInventoryChangeMessage Change) { Refresh(); }
void UNGInventoryPanel::EquipmentChanged(FNGEquipmentState Equipment) { Refresh(); }

void UNGInventoryPanel::Refresh()
{
	UnbindItems();
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
			SlotInfo.bEquipped = Combat.IsValid() && Combat->GetEquipment().bEquipped && Combat->GetEquippedItem() == Item;
			Item->OnStatsChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
			ObservedItems.Add(Item);
		}
	}
	SetInventoryData(InventoryTitle, Slots, this);
}

bool UNGInventoryPanel::RequestInventoryOperation_Implementation(const FUIInventorySlotData& SlotInfo, EUIInventoryOperation Operation, int32 Amount)
{
	auto* PC = GetOwningPlayer();
	const auto* PS = PC ? PC->GetPlayerState<APlayerState>() : nullptr;
	auto* Item = Cast<UNGInventoryItemInstance>(SlotInfo.Item.Get());
	// UI snapshots are untrusted: resolve ownership/count again, then use existing server-validated requests.
	if (!PS || !Inventory.IsValid() || (Inventory->GetOwner() != PS && Inventory->GetOwner() != PC->GetPawn())
		|| SlotInfo.Inventory != Inventory.Get() || Inventory->GetItemAtSlot(SlotInfo.SlotIndex) != Item || !Inventory->Contains(Item) || Amount < 0 || Amount > Inventory->GetCount(Item)) return false;
	if (Operation == EUIInventoryOperation::PrimaryAction)
	{
		if (!Inventory->GetSlotTag(SlotInfo.SlotIndex).MatchesTag(TAG_NGSlotAction)
			|| !Combat.IsValid() || Combat->GetOwner() != PC->GetPawn() || !Item->GetItemDef()
			|| !UNGCombatComponent::GetWeaponDefinition(GetDefault<UNGInventoryItemDefinition>(Item->GetItemDef()))) return false;
		Combat->RequestEquipItem(Item);
		return true;
	}
	if (Operation == EUIInventoryOperation::Drop)
		if (auto* Transport = PlayerInventory.Get())
		{
			Transport->RequestDropItem(Item, Amount);
			return true;
		}
	return false;
}

bool UNGInventoryPanel::MoveInventoryItem_Implementation(const FUIInventorySlotData& Source, const FUIInventorySlotData& Destination, int32 Amount)
{
	auto* Transport = PlayerInventory.Get();
	auto* SourceInventory = Cast<UNGInventoryManagerComponent>(Source.Inventory.Get());
	auto* TargetInventory = Cast<UNGInventoryManagerComponent>(Destination.Inventory.Get());
	auto* Item = Cast<UNGInventoryItemInstance>(Source.Item.Get());
	auto* ExpectedTarget = Cast<UNGInventoryItemInstance>(Destination.Item.Get());
	if (!Transport || !IsValid(SourceInventory) || !IsValid(TargetInventory) || TargetInventory != Inventory.Get()
		|| !IsValid(Item) || SourceInventory->GetItemAtSlot(Source.SlotIndex) != Item || Amount < 0 || Amount > SourceInventory->GetCount(Item)
		|| TargetInventory->GetItemAtSlot(Destination.SlotIndex) != ExpectedTarget) return false;
	Transport->RequestInventoryTransfer(SourceInventory, Item, Source.SlotIndex, TargetInventory, Destination.SlotIndex, ExpectedTarget, Amount);
	return true;
}
