// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#include "Inventory/NGInventoryManagerComponent.h"

#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Inventory/NGInventoryLayoutDefinition.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_NGSlotStorage, "Inventory.Slot.Storage");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGSlotAction, "Inventory.Slot.Action");
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NGInventoryManagerComponent)


struct FReplicationFlags;


//////////////////////////////////////////////////////////////////////
// FNGInventoryEntry

FString FNGInventoryEntry::GetDebugString() const
{
	TSubclassOf<UNGInventoryItemDefinition> ItemDef;
	if (Instance != nullptr)
	{
		ItemDef = Instance->GetItemDef();
	}

	return FString::Printf(TEXT("%s (%d x %s)"), *GetNameSafe(Instance), StackCount, *GetNameSafe(ItemDef));
}

//////////////////////////////////////////////////////////////////////
// FNGInventoryList
void FNGInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (auto* Manager = Cast<UNGInventoryManagerComponent>(OwnerComponent))
	{
		FNGInventoryChangeMessage Message; Message.InventoryOwner = OwnerComponent;
		Manager->OnInventoryChanged.Broadcast(Message); // Also refresh after item NetGUIDs become mapped.
	}
}

void FNGInventoryList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		FNGInventoryEntry& Stack = Entries[Index];
		BroadcastChangeMessage(Stack, /*OldCount=*/ Stack.StackCount, /*NewCount=*/ 0);
		Stack.LastObservedCount = 0;
	}
}

void FNGInventoryList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		FNGInventoryEntry& Stack = Entries[Index];
		BroadcastChangeMessage(Stack, /*OldCount=*/ 0, /*NewCount=*/ Stack.StackCount);
		Stack.LastObservedCount = Stack.StackCount;
	}
}

void FNGInventoryList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		FNGInventoryEntry& Stack = Entries[Index];
		BroadcastChangeMessage(Stack, /*OldCount=*/ Stack.LastObservedCount, /*NewCount=*/ Stack.StackCount);
		Stack.LastObservedCount = Stack.StackCount;
	}
}

void FNGInventoryList::BroadcastChangeMessage(FNGInventoryEntry& Entry, int32 OldCount, int32 NewCount)
{
	FNGInventoryChangeMessage Message;
	Message.InventoryOwner = OwnerComponent;
	Message.Instance = Entry.Instance;
	Message.NewCount = NewCount;
	Message.Delta = NewCount - OldCount;
	Message.SlotIndex = Entry.SlotIndex;

	if (auto* Manager = Cast<UNGInventoryManagerComponent>(OwnerComponent)) Manager->OnInventoryChanged.Broadcast(Message);
}

UNGInventoryItemInstance* FNGInventoryList::AddEntry(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount, const TMap<FGameplayTag, int32>* RestoredStats, bool bNotify, int32 SlotIndex)
{
    auto* Manager = Cast<UNGInventoryManagerComponent>(OwnerComponent);
    if (!Manager || !Manager->GetOwner()->HasAuthority() || !ItemDef || StackCount < 1) return nullptr;
    const int32 Destination = SlotIndex == INDEX_NONE ? Manager->FindFirstAvailableSlot(ItemDef) : SlotIndex;
    if (!Manager->CanPlaceItemAtSlot(ItemDef, Destination) || Manager->GetItemAtSlot(Destination)) return nullptr;
    auto* Item = NewObject<UNGInventoryItemInstance>(Manager->GetOwner());
    Item->SetItemDef(ItemDef);
    for (UNGInventoryItemFragment* Fragment : GetDefault<UNGInventoryItemDefinition>(ItemDef)->Fragments)
        if (Fragment) Fragment->OnInstanceCreated(Item);
    if (RestoredStats) Item->RestoreStatStacks(*RestoredStats);
    // Fragments may call gameplay code: resolve and validate the destination again.
    auto* Entry = Manager->FindSlot(Destination);
    if (!Entry || Entry->Instance || !Manager->CanPlaceItemAtSlot(ItemDef, Destination)) return nullptr;
    Entry->Instance = Item;
    Entry->StackCount = StackCount;
    MarkItemDirty(*Entry);
    if (bNotify) BroadcastChangeMessage(*Entry, 0, StackCount);
    return Item;
}

void FNGInventoryList::AddEntry(UNGInventoryItemInstance* Instance)
{
    auto* Manager = Cast<UNGInventoryManagerComponent>(OwnerComponent);
    if (!Instance || !Manager || !Manager->GetOwner()->HasAuthority()
        || Instance->GetOuter() != Manager->GetOwner() || Manager->Contains(Instance)) return;
    auto* Entry = Manager->FindSlot(Manager->FindFirstAvailableSlot(Instance->GetItemDef()));
    if (!Entry) return;
    Entry->Instance = Instance; Entry->StackCount = 1;
    MarkItemDirty(*Entry);
    BroadcastChangeMessage(*Entry, 0, 1);
}

void FNGInventoryList::RemoveEntry(UNGInventoryItemInstance* Instance, bool bNotify)
{
    if (!Instance) return;
    for (auto& Entry : Entries) if (Entry.Instance == Instance)
    {
        const auto Previous = Entry;
        Entry.Instance = nullptr;
        Entry.StackCount = 0;
        MarkItemDirty(Entry); // Empty slot and its rules remain in the array.
        if (bNotify)
        {
            auto MessageEntry = Previous;
            BroadcastChangeMessage(MessageEntry, Previous.StackCount, 0);
        }
        return;
    }
}

TArray<UNGInventoryItemInstance*> FNGInventoryList::GetAllItems() const
{
	TArray<UNGInventoryItemInstance*> Results;
	Results.Reserve(Entries.Num());
	for (const FNGInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr) //@TODO: Would prefer to not deal with this here and hide it further?
		{
			Results.Add(Entry.Instance);
		}
	}
	return Results;
}

//////////////////////////////////////////////////////////////////////
// UNGInventoryManagerComponent

UNGInventoryManagerComponent::UNGInventoryManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InventoryList(this)
{
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;
}

void UNGInventoryManagerComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, InventoryList, COND_Dynamic);
}

bool UNGInventoryManagerComponent::CanAddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount)
{
	return GetOwner() && GetOwner()->HasAuthority() && InitializeInventory() && ItemDef && StackCount > 0
		&& StackCount <= GetDefault<UNGInventoryItemDefinition>(ItemDef)->MaxStackSize && FindFirstAvailableSlot(ItemDef) != INDEX_NONE;
}

UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount)
{
	UNGInventoryItemInstance* Result = nullptr;
	if (CanAddItemDefinition(ItemDef, StackCount))
	{
		Result = InventoryList.AddEntry(ItemDef, StackCount);

		if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && Result)
		{
			AddReplicatedSubObject(Result, bOwnerOnly ? COND_OwnerOnly : COND_None);
		}
	}
	return Result;
}

UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemDefinitionToSlot(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 SlotIndex, int32 StackCount)
{
	if (!CanAddItemDefinition(ItemDef, StackCount) || !CanPlaceItemAtSlot(ItemDef, SlotIndex) || GetItemAtSlot(SlotIndex)) return nullptr;
	// Run item fragments (including initial magazine stats), then publish only the final slot.
	auto* Item = InventoryList.AddEntry(ItemDef, StackCount, nullptr, false, SlotIndex);
	if (!Item) return nullptr;
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication()) AddReplicatedSubObject(Item, bOwnerOnly ? COND_OwnerOnly : COND_None);
	NotifyItemChange(Item, 0, StackCount);
	return Item;
}

void UNGInventoryManagerComponent::AddItemInstance(UNGInventoryItemInstance* ItemInstance)
{
	if (!InitializeInventory()) return;
	if (!GetOwner()->HasAuthority() || !ItemInstance || ItemInstance->GetOuter() != GetOwner() || Contains(ItemInstance) || FindFirstAvailableSlot(ItemInstance->GetItemDef()) == INDEX_NONE) return;
	TArray<UNGInventoryManagerComponent*> Containers;
	GetOwner()->GetComponents(Containers);
	for (const auto* Container : Containers) if (Container->Contains(ItemInstance)) return;
	InventoryList.AddEntry(ItemInstance);
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && ItemInstance)
	{
		AddReplicatedSubObject(ItemInstance, bOwnerOnly ? COND_OwnerOnly : COND_None);
	}
}

void UNGInventoryManagerComponent::RemoveItemInstance(UNGInventoryItemInstance* ItemInstance)
{
	if (!GetOwner()->HasAuthority()) return;
	InventoryList.RemoveEntry(ItemInstance);

	if (ItemInstance && IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}
}

TArray<UNGInventoryItemInstance*> UNGInventoryManagerComponent::GetAllItems() const
{
	return InventoryList.GetAllItems();
}

UNGInventoryItemInstance* UNGInventoryManagerComponent::FindFirstItemStackByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef) const
{
	for (const FNGInventoryEntry& Entry : InventoryList.Entries)
	{
		UNGInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				return Instance;
			}
		}
	}

	return nullptr;
}

int32 UNGInventoryManagerComponent::GetTotalItemCountByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef) const
{
	int32 TotalCount = 0;
	for (const FNGInventoryEntry& Entry : InventoryList.Entries)
	{
		UNGInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				TotalCount += Entry.StackCount;
			}
		}
	}

	return TotalCount;
}

bool UNGInventoryManagerComponent::ConsumeItemsByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 NumToConsume)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return false;
	}

	if (!ItemDef || NumToConsume < 1 || GetTotalItemCountByDefinition(ItemDef) < NumToConsume) return false;
	for (int32 Index = InventoryList.Entries.Num()-1; Index >= 0 && NumToConsume > 0; --Index)
	{
		auto& Entry = InventoryList.Entries[Index];
		if (!Entry.Instance || Entry.Instance->GetItemDef() != ItemDef) continue;
		const int32 Count = FMath::Min(Entry.StackCount, NumToConsume); NumToConsume -= Count;
		if (Count == Entry.StackCount) RemoveItemInstance(Entry.Instance);
		else { const int32 Old = Entry.StackCount; Entry.StackCount -= Count; InventoryList.MarkItemDirty(Entry); InventoryList.BroadcastChangeMessage(Entry, Old, Entry.StackCount); }
	}
	return true;
}

void UNGInventoryManagerComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	// Register existing UNGInventoryItemInstance
	if (IsUsingRegisteredSubObjectList())
	{
		for (const FNGInventoryEntry& Entry : InventoryList.Entries)
		{
			UNGInventoryItemInstance* Instance = Entry.Instance;

			if (IsValid(Instance))
			{
				AddReplicatedSubObject(Instance, bOwnerOnly ? COND_OwnerOnly : COND_None);
			}
		}
	}
}

bool UNGInventoryManagerComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	if (bOwnerOnly && !RepFlags->bNetOwner) return WroteSomething;

	for (FNGInventoryEntry& Entry : InventoryList.Entries)
	{
		UNGInventoryItemInstance* Instance = Entry.Instance;

		if (Instance && IsValid(Instance))
		{
			WroteSomething |= Channel->ReplicateSubobject(Instance, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}

bool UNGInventoryManagerComponent::HasResolvedItems() const
{
	for (const auto& Entry : InventoryList.Entries)
		if (Entry.StackCount > 0 && (!IsValid(Entry.Instance) || !Entry.Instance->GetItemDef())) return false;
	return true;
}

bool UNGInventoryManagerComponent::Contains(UNGInventoryItemInstance* Item) const
{
	return Item && InventoryList.Entries.ContainsByPredicate([Item](const auto& Entry) { return Entry.Instance == Item; });
}
int32 UNGInventoryManagerComponent::GetCount(UNGInventoryItemInstance* Item) const
{
	for (const auto& Entry : InventoryList.Entries) if (Entry.Instance == Item) return Entry.StackCount;
	return 0;
}
UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemState(const FNGItemState& State, bool bNotify, int32 SlotIndex)
{
	if (!CanAddItemDefinition(State.ItemClass, State.Count)) return nullptr;
	if (SlotIndex != INDEX_NONE && (!CanPlaceItemAtSlot(State.ItemClass, SlotIndex) || GetItemAtSlot(SlotIndex))) return nullptr;
	for (const auto& Pair : State.StatStacks) if (!Pair.Key.IsValid() || Pair.Value < 0) return nullptr;
	auto* Item = InventoryList.AddEntry(State.ItemClass, State.Count, &State.StatStacks, false, SlotIndex);
	if (Item && bNotify) NotifyItemChange(Item, 0, State.Count);
	if (Item && IsUsingRegisteredSubObjectList() && IsReadyForReplication()) AddReplicatedSubObject(Item, bOwnerOnly ? COND_OwnerOnly : COND_None);
	return Item;
}
UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemDefinitionDeferred(TSubclassOf<UNGInventoryItemDefinition> ItemClass,int32 Count)
{
	if (!CanAddItemDefinition(ItemClass,Count)) return nullptr;
	auto* Item=InventoryList.AddEntry(ItemClass,Count,nullptr,false);
	if (Item && IsUsingRegisteredSubObjectList() && IsReadyForReplication()) AddReplicatedSubObject(Item,bOwnerOnly ? COND_OwnerOnly : COND_None);
	return Item;
}
bool UNGInventoryManagerComponent::RemoveQuantity(UNGInventoryItemInstance* Item, int32 Count, bool bNotify)
{
	if (!GetOwner()->HasAuthority() || Count < 1 || !Contains(Item) || GetCount(Item) < Count) return false;
	for (auto& Entry : InventoryList.Entries) if (Entry.Instance == Item)
	{
		const int32 Old = Entry.StackCount;
		if (Old == Count)
		{
			InventoryList.RemoveEntry(Item, bNotify);
			if (IsUsingRegisteredSubObjectList()) RemoveReplicatedSubObject(Item);
		}
		else
		{
			Entry.StackCount -= Count; InventoryList.MarkItemDirty(Entry);
			if (bNotify) NotifyItemChange(Item, Old, Old-Count);
		}
		return true;
	}
	return false;
}
void UNGInventoryManagerComponent::NotifyItemChange(UNGInventoryItemInstance* Item, int32 OldCount, int32 NewCount, int32 SlotIndex)
{
	FNGInventoryChangeMessage Message; Message.InventoryOwner = this; Message.Instance = Item;
	Message.NewCount = NewCount; Message.Delta = NewCount-OldCount;
	Message.SlotIndex = SlotIndex == INDEX_NONE ? GetSlotIndex(Item) : SlotIndex;
	OnInventoryChanged.Broadcast(Message);
}

void UNGInventoryManagerComponent::GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const
{
	Super::GetReplicatedCustomConditionState(OutActiveState);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(ThisClass, InventoryList, bOwnerOnly ? COND_OwnerOnly : COND_None);
}

void UNGInventoryManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();
	if (GetOwner()->HasAuthority()) InitializeInventory();
}

bool UNGInventoryManagerComponent::InitializeInventory()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	if (bSlotsInitialized) return true;
	if (!Layout)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: assign an Inventory Layout before initializing slots."), *GetPathName());
		return false;
	}
	InventoryList.OwnerComponent = this;
	InventoryList.Entries.Reset();
	InventoryList.Entries.SetNum(Layout->Slots.Num());
	for (int32 Index = 0; Index < Layout->Slots.Num(); ++Index)
	{
		auto& Entry = InventoryList.Entries[Index];
		Entry.SlotIndex = Index;
		Entry.SlotTag = Layout->Slots[Index].SlotTag;
		Entry.AcceptedItems = Layout->Slots[Index].AcceptedItems;
		InventoryList.MarkItemDirty(Entry); // Replicate empty slots and their rules too.
	}
	bSlotsInitialized = true; // Empty layouts are initialized too; never rebuild an occupied inventory.
	FNGInventoryChangeMessage Message; Message.InventoryOwner = this;
	OnInventoryChanged.Broadcast(Message);
	return true;
}

FNGInventoryEntry* UNGInventoryManagerComponent::FindSlot(int32 SlotIndex)
{
    if (SlotIndex < 0) return nullptr;
    return InventoryList.Entries.FindByPredicate([SlotIndex](const auto& Entry) { return Entry.SlotIndex == SlotIndex; });
}
const FNGInventoryEntry* UNGInventoryManagerComponent::FindSlot(int32 SlotIndex) const
{
    if (SlotIndex < 0) return nullptr;
    return InventoryList.Entries.FindByPredicate([SlotIndex](const auto& Entry) { return Entry.SlotIndex == SlotIndex; });
}

UNGInventoryItemInstance* UNGInventoryManagerComponent::GetItemAtSlot(int32 SlotIndex) const
{
    const auto* Entry = FindSlot(SlotIndex);
    return Entry ? Entry->Instance.Get() : nullptr;
}
int32 UNGInventoryManagerComponent::GetSlotIndex(UNGInventoryItemInstance* Item) const
{
    if (!Item) return INDEX_NONE;
    for (const auto& Entry : InventoryList.Entries) if (Entry.Instance == Item) return Entry.SlotIndex;
    return INDEX_NONE;
}
int32 UNGInventoryManagerComponent::FindFirstEmptySlot() const
{
    for (int32 Index = 0; Index < GetSlotCount(); ++Index)
        if (const auto* Entry = FindSlot(Index); Entry && Entry->StackCount == 0 && !Entry->Instance) return Index;
    return INDEX_NONE;
}
int32 UNGInventoryManagerComponent::FindFirstAvailableSlot(TSubclassOf<UNGInventoryItemDefinition> ItemClass) const
{
    for (int32 Index = 0; Index < GetSlotCount(); ++Index)
        if (const auto* Entry = FindSlot(Index); Entry && Entry->StackCount == 0 && !Entry->Instance && CanPlaceItemAtSlot(ItemClass, Index)) return Index;
    return INDEX_NONE;
}
FGameplayTag UNGInventoryManagerComponent::GetSlotTag(int32 SlotIndex) const
{
    const auto* Entry = FindSlot(SlotIndex);
    return Entry ? Entry->SlotTag : FGameplayTag();
}
int32 UNGInventoryManagerComponent::FindSlotByTag(FGameplayTag SlotTag, int32 Ordinal) const
{
	if (!SlotTag.IsValid() || Ordinal < 0) return INDEX_NONE;
	for (int32 Index = 0; Index < GetSlotCount(); ++Index)
		if (GetSlotTag(Index).MatchesTag(SlotTag) && Ordinal-- == 0) return Index;
	return INDEX_NONE;
}
bool UNGInventoryManagerComponent::CanPlaceItemAtSlot(TSubclassOf<UNGInventoryItemDefinition> ItemClass, int32 SlotIndex) const
{
    const auto* Entry = FindSlot(SlotIndex);
    return Entry && ItemClass && !ItemClass->HasAnyClassFlags(CLASS_Abstract)
        && (Entry->AcceptedItems.IsEmpty() || Entry->AcceptedItems.Matches(GetDefault<UNGInventoryItemDefinition>(ItemClass)->ItemTags));
}

bool UNGInventoryManagerComponent::MoveItemToSlot(UNGInventoryItemInstance* Item, int32 SlotIndex)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || bTransferring || !Contains(Item) || !CanPlaceItemAtSlot(Item->GetItemDef(), SlotIndex)) return false;
    const int32 OldSlot = GetSlotIndex(Item);
    if (OldSlot == SlotIndex) return true;
    auto* SourceEntry = FindSlot(OldSlot);
    auto* TargetEntry = FindSlot(SlotIndex);
    auto* Other = TargetEntry->Instance.Get();
    if (Other && !CanPlaceItemAtSlot(Other->GetItemDef(), OldSlot)) return false;
    TGuardValue<bool> Guard(bTransferring, true);
    Swap(SourceEntry->Instance, TargetEntry->Instance);
    Swap(SourceEntry->StackCount, TargetEntry->StackCount);
    InventoryList.MarkItemDirty(*SourceEntry);
    InventoryList.MarkItemDirty(*TargetEntry);
    // Slot tags, acceptance rules and replication identities never move with the item.
    NotifyItemChange(Item, GetCount(Item), GetCount(Item));
    if (Other) NotifyItemChange(Other, GetCount(Other), GetCount(Other));
    return true;
}
UNGInventoryItemInstance* UNGInventoryManagerComponent::ImportItemState(const FNGItemState& State, int32 SlotIndex)
{
	if (SlotIndex < 0 || bTransferring) return nullptr;
	return AddItemState(State, true, SlotIndex);
}
TArray<FNGInventorySlotState> UNGInventoryManagerComponent::CaptureInventoryState() const
{
	TArray<FNGInventorySlotState> Result;
	for (const auto& Entry : InventoryList.Entries) if (Entry.Instance)
	{
		auto& State = Result.AddDefaulted_GetRef(); State.SlotIndex = Entry.SlotIndex;
		State.Item = Entry.Instance->CaptureState(Entry.StackCount);
	}
	return Result;
}

bool UNGInventoryManagerComponent::TransferTo(UNGInventoryManagerComponent* Target, int32 SourceSlot, int32 TargetSlot, int32 Amount)
{
	if (!IsValid(Target) || !GetOwner() || !GetOwner()->HasAuthority() || !Target->GetOwner() || !Target->GetOwner()->HasAuthority()
		|| GetWorld() != Target->GetWorld() || bTransferring || Target->bTransferring || Amount < 0) return false;
	auto* Item = GetItemAtSlot(SourceSlot);
	if (!Item || !Target->CanPlaceItemAtSlot(Item->GetItemDef(), TargetSlot)) return false;
	const int32 OldCount = GetCount(Item);
	const int32 Count = Amount == 0 ? OldCount : Amount;
	if (Count < 1 || Count > OldCount) return false;
	if (Target == this && SourceSlot == TargetSlot) return true;
	auto* Other = Target->GetItemAtSlot(TargetSlot);
	FNGItemState State = Item->CaptureState(Count);
	if (Count < OldCount && !State.CanSplitStack()) return false;
	const bool bMerge = Other && State.CanMergeStackWith(Other->CaptureState());
	if (!bMerge && Target == this && Count == OldCount) return MoveItemToSlot(Item, TargetSlot);
	// Cross-container swapping is intentionally rejected; move to an empty slot or merge compatible stacks.
	if (Other && !bMerge) return false;
	const int32 TargetCount = Other ? Target->GetCount(Other) : 0;
	if (bMerge && Count > GetDefault<UNGInventoryItemDefinition>(Item->GetItemDef())->MaxStackSize - TargetCount) return false;
	TGuardValue<bool> SourceGuard(bTransferring, true);
	TGuardValue<bool> TargetGuard(Target->bTransferring, true);
	UNGInventoryItemInstance* DestinationItem = Other;
	if (!bMerge)
	{
		DestinationItem = Target->AddItemState(State, false, TargetSlot);
		if (!DestinationItem) return false;
	}
	// Revalidate after fragment initialization before committing the source removal.
	if (GetItemAtSlot(SourceSlot) != Item || GetCount(Item) != OldCount
		|| !Item->CaptureState(Count).StatStacks.OrderIndependentCompareEqual(State.StatStacks)
		|| !RemoveQuantity(Item, Count, false))
	{
		if (!bMerge) Target->RemoveQuantity(DestinationItem, Count, false);
		return false;
	}
	if (bMerge)
		for (auto& Entry : Target->InventoryList.Entries) if (Entry.Instance == Other)
		{
			Entry.StackCount += Count; Target->InventoryList.MarkItemDirty(Entry); break;
		}
	NotifyItemChange(Item, OldCount, OldCount - Count, SourceSlot);
	Target->NotifyItemChange(DestinationItem, TargetCount, TargetCount + Count, TargetSlot);
	return true;
}
