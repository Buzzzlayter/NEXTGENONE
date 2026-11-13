
#include "Inventory/NGInventoryManagerComponent.h"
#include "Engine/ActorChannel.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Inventory/NGInventoryLayoutDefinition.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "UObject/StrongObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NGInventoryManagerComponent)

UE_DEFINE_GAMEPLAY_TAG(TAG_NGSlotStorage, "Inventory.Slot.Storage");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGSlotAction, "Inventory.Slot.Action");

struct FReplicationFlags;

// Keep mutation callbacks out of partial state, including fragment initialization.
struct UNGInventoryManagerComponent::FMutationScope
{
	UNGInventoryManagerComponent* Source;
	UNGInventoryManagerComponent* Target;
	explicit FMutationScope(UNGInventoryManagerComponent* InSource, UNGInventoryManagerComponent* InTarget = nullptr)
		: Source(InSource), Target(InTarget == InSource ? nullptr : InTarget)
	{
		Source->bTransferring = true;
		if (Target) Target->bTransferring = true;
	}
	~FMutationScope()
	{
		// Listeners see the final state, but cannot mutate either participant reentrantly.
		Source->PublishChanges();
		if (Target) Target->PublishChanges();
		Source->bTransferring = false;
		if (Target) Target->bTransferring = false;
	}
};

FString FNGInventoryEntry::GetDebugString() const
{
	TSubclassOf<UNGInventoryItemDefinition> ItemDef;
	if (Instance != nullptr)
	{
		ItemDef = Instance->GetItemDef();
	}

	return FString::Printf(TEXT("%s (%d x %s)"), *GetNameSafe(Instance), StackCount, *GetNameSafe(ItemDef));
}

void FNGInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (auto* Manager = Cast<UNGInventoryManagerComponent>(OwnerComponent)) Manager->NotifyInventoryChanged();
}

UNGInventoryItemInstance* FNGInventoryList::CreateInstance(TSubclassOf<UNGInventoryItemDefinition> ItemDef, const TMap<FGameplayTag, int32>* RestoredStats)
{
	auto* Manager = Cast<UNGInventoryManagerComponent>(OwnerComponent);
	if (!Manager || !Manager->GetOwner()->HasAuthority() || !ItemDef) return nullptr;
	auto* Item = NewObject<UNGInventoryItemInstance>(Manager->GetOwner());
	Item->SetItemDef(ItemDef);
	for (UNGInventoryItemFragment* Fragment : GetDefault<UNGInventoryItemDefinition>(ItemDef)->Fragments)
		if (Fragment) Fragment->OnInstanceCreated(Item);
	if (RestoredStats) Item->RestoreStatStacks(*RestoredStats);
	return Item;
}

UNGInventoryItemInstance* FNGInventoryList::AddEntry(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount, const TMap<FGameplayTag, int32>* RestoredStats, bool bNotify, int32 SlotIndex)
{
	auto* Manager = Cast<UNGInventoryManagerComponent>(OwnerComponent);
	if (!Manager || !Manager->GetOwner()->HasAuthority() || !ItemDef || StackCount < 1) return nullptr;
	const int32 Destination = SlotIndex == INDEX_NONE ? Manager->FindFirstAvailableSlot(ItemDef) : SlotIndex;
	if (!Manager->CanPlaceItemAtSlot(ItemDef, Destination) || Manager->GetItemAtSlot(Destination)) return nullptr;
	auto* Item = CreateInstance(ItemDef, RestoredStats);
	if (!Item) return nullptr;
	// Fragments may call gameplay code: resolve and validate the destination again.
	auto* Entry = Manager->FindSlot(Destination);
	if (!Entry || Entry->Instance || !Manager->CanPlaceItemAtSlot(ItemDef, Destination)) return nullptr;
	Entry->Instance = Item;
	Entry->StackCount = StackCount;
	MarkItemDirty(*Entry);
	if (bNotify) Manager->NotifyInventoryChanged();
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
}

void FNGInventoryList::RemoveEntry(UNGInventoryItemInstance* Instance, bool bNotify)
{
	if (!Instance) return;
	for (auto& Entry : Entries) if (Entry.Instance == Instance)
	{
		Entry.Instance = nullptr;
		Entry.StackCount = 0;
		MarkItemDirty(Entry); // Empty slot and its rules remain in the array.
		if (bNotify)
		{
			if (auto* Manager = Cast<UNGInventoryManagerComponent>(OwnerComponent)) Manager->NotifyInventoryChanged();
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
		// Empty slots remain in the replicated list but are excluded from the item view.
		if (Entry.Instance != nullptr)
		{
			Results.Add(Entry.Instance);
		}
	}
	return Results;
}

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

bool UNGInventoryManagerComponent::CanAddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount) const
{
	return FindFirstAvailableSlot(ItemDef, StackCount) != INDEX_NONE;
}

UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount)
{
	if (bTransferring || !InitializeInventory()) return nullptr;
	FMutationScope Mutation(this);
	return AddItemInternal(ItemDef, StackCount, nullptr, true, INDEX_NONE);
}

UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemDefinitionToSlot(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 SlotIndex, int32 StackCount)
{
	if (bTransferring || !InitializeInventory()) return nullptr;
	FMutationScope Mutation(this);
	return AddItemInternal(ItemDef, StackCount, nullptr, true, SlotIndex);
}

void UNGInventoryManagerComponent::AddItemInstance(UNGInventoryItemInstance* ItemInstance)
{
	if (bTransferring || !InitializeInventory() || !IsValid(ItemInstance) || ItemInstance->GetOuter() != GetOwner()
		|| Contains(ItemInstance) || !CanAddItemDefinition(ItemInstance->GetItemDef())) return;
	TArray<UNGInventoryManagerComponent*> Containers;
	GetOwner()->GetComponents(Containers);
	for (const auto* Container : Containers) if (Container->Contains(ItemInstance)) return;
	FMutationScope Mutation(this);
	InventoryList.AddEntry(ItemInstance);
	if (!Contains(ItemInstance)) return;
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication()) AddReplicatedSubObject(ItemInstance, bOwnerOnly ? COND_OwnerOnly : COND_None);
	NotifyInventoryChanged();
}

void UNGInventoryManagerComponent::RemoveItemInstance(UNGInventoryItemInstance* ItemInstance)
{
	RemoveQuantity(ItemInstance, GetCount(ItemInstance));
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
				if (Entry.StackCount > MAX_int32 - TotalCount) return MAX_int32;
				TotalCount += Entry.StackCount;
			}
		}
	}

	return TotalCount;
}

bool UNGInventoryManagerComponent::ConsumeItemsByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 NumToConsume)
{
	return ConsumeItemsByDefinition(ItemDef, NumToConsume, [] {});
}

bool UNGInventoryManagerComponent::ConsumeItemsByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 NumToConsume, TFunctionRef<void()> OnConsumed)
{
	if (bTransferring || !GetOwner() || !GetOwner()->HasAuthority() || !ItemDef || NumToConsume < 1
		|| GetTotalItemCountByDefinition(ItemDef) < NumToConsume) return false;
	TArray<TTuple<UNGInventoryItemInstance*, int32>> Debits;
	int32 Remaining = NumToConsume;
	for (auto* Item : GetAllItems())
	{
		if (Remaining == 0) break;
		if (Item->GetItemDef() != ItemDef) continue;
		const int32 OldCount = GetCount(Item);
		const int32 Take = FMath::Min(OldCount, Remaining);
		if (Take < OldCount && !Item->CaptureState().CanSplitStack()) return false;
		Debits.Emplace(Item, Take);
		Remaining -= Take;
	}
	FMutationScope Mutation(this);
	// No callbacks occur between preflight and these private mutations.
	for (const auto& Debit : Debits) RemoveQuantityInternal(Debit.Get<0>(), Debit.Get<1>(), true);
	OnConsumed();
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
	if (bTransferring || !InitializeInventory()) return nullptr;
	FMutationScope Mutation(this);
	return AddItemInternal(State.ItemClass, State.Count, &State.StatStacks, bNotify, SlotIndex);
}

UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemInternal(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 Count, const TMap<FGameplayTag, int32>* Stats, bool bNotify, int32 SlotIndex)
{
	if (!CanAddItemDefinition(ItemDef, Count)) return nullptr;
	if (SlotIndex != INDEX_NONE && (!CanPlaceItemAtSlot(ItemDef, SlotIndex) || GetItemAtSlot(SlotIndex))) return nullptr;
	if (Stats) for (const auto& Pair : *Stats) if (!Pair.Key.IsValid() || Pair.Value < 0) return nullptr;
	auto* Item = InventoryList.AddEntry(ItemDef, Count, Stats, false, SlotIndex);
	if (!Item) return nullptr;
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication()) AddReplicatedSubObject(Item, bOwnerOnly ? COND_OwnerOnly : COND_None);
	if (bNotify) NotifyInventoryChanged();
	return Item;
}

UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemDefinitionDeferred(TSubclassOf<UNGInventoryItemDefinition> ItemClass, int32 Count, int32 SlotIndex)
{
	if (bTransferring || !InitializeInventory()) return nullptr;
	FMutationScope Mutation(this);
	return AddItemInternal(ItemClass, Count, nullptr, false, SlotIndex);
}

bool UNGInventoryManagerComponent::RemoveQuantity(UNGInventoryItemInstance* Item, int32 Count, bool bNotify)
{
	if (bTransferring || !GetOwner() || !GetOwner()->HasAuthority()) return false;
	FMutationScope Mutation(this);
	return RemoveQuantityInternal(Item, Count, bNotify);
}

bool UNGInventoryManagerComponent::ConsumeItem(UNGInventoryItemInstance* Item, int32 Count, TFunctionRef<void()> OnConsumed)
{
	if (bTransferring || !GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Item)
		|| Count < 1 || GetCount(Item) < Count || (GetCount(Item) > Count && !Item->CaptureState().CanSplitStack())) return false;
	FMutationScope Mutation(this);
	if (!RemoveQuantityInternal(Item, Count, true)) return false;
	OnConsumed();
	return true;
}

bool UNGInventoryManagerComponent::RemoveQuantityInternal(UNGInventoryItemInstance* Item, int32 Count, bool bNotify)
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
			if (bNotify) NotifyInventoryChanged();
		}
		return true;
	}
	return false;
}
void UNGInventoryManagerComponent::NotifyInventoryChanged()
{
	bInventoryDirty = true;
	if (!bTransferring) PublishChanges();
}

void UNGInventoryManagerComponent::PublishChanges()
{
	if (!bInventoryDirty) return;
	bInventoryDirty = false;
	OnInventoryChanged.Broadcast();
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
	NotifyInventoryChanged();
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
int32 UNGInventoryManagerComponent::FindFirstAvailableSlot(TSubclassOf<UNGInventoryItemDefinition> ItemClass, int32 StackCount, FGameplayTag SlotTag) const
{
	if (!ItemClass || ItemClass->HasAnyClassFlags(CLASS_Abstract) || StackCount < 1
		|| StackCount > GetDefault<UNGInventoryItemDefinition>(ItemClass)->MaxStackSize) return INDEX_NONE;
	FString Error;
	if (!GetDefault<UNGInventoryItemDefinition>(ItemClass)->ValidateDefinition(Error)) return INDEX_NONE;
	for (int32 Index = 0; Index < GetSlotCount(); ++Index)
		if (const auto* Entry = FindSlot(Index); Entry && Entry->StackCount == 0 && !Entry->Instance
			&& (!SlotTag.IsValid() || Entry->SlotTag.MatchesTag(SlotTag)) && CanPlaceItemAtSlot(ItemClass, Index)) return Index;
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
	FMutationScope Mutation(this);
	Swap(SourceEntry->Instance, TargetEntry->Instance);
	Swap(SourceEntry->StackCount, TargetEntry->StackCount);
	InventoryList.MarkItemDirty(*SourceEntry);
	InventoryList.MarkItemDirty(*TargetEntry);
	// Slot tags, acceptance rules and replication identities never move with the item.
	NotifyInventoryChanged();
	return true;
}
UNGInventoryItemInstance* UNGInventoryManagerComponent::ImportItemState(const FNGItemState& State, int32 SlotIndex)
{
	if (SlotIndex < INDEX_NONE || bTransferring) return nullptr;
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
	const int32 TargetCount = Other ? Target->GetCount(Other) : 0;
	const FNGItemState OtherState = Other ? Other->CaptureState(TargetCount) : FNGItemState();
	const int32 MaxStackSize = GetDefault<UNGInventoryItemDefinition>(State.ItemClass)->MaxStackSize;
	const bool bMerge = Other && State.CanMergeStackWith(OtherState) && Count <= MaxStackSize - TargetCount;
	if (!bMerge && Target == this && Count == OldCount) return MoveItemToSlot(Item, TargetSlot);
	const bool bSwap = Other && !bMerge;
	if (bSwap && (Count != OldCount || !CanPlaceItemAtSlot(OtherState.ItemClass, SourceSlot))) return false;
	if (Count > MaxStackSize || State.ItemClass->HasAnyClassFlags(CLASS_Abstract)) return false;
	if (bSwap && (TargetCount < 1 || TargetCount > GetDefault<UNGInventoryItemDefinition>(OtherState.ItemClass)->MaxStackSize
		|| OtherState.ItemClass->HasAnyClassFlags(CLASS_Abstract))) return false;
	FMutationScope Mutation(this, Target);
	FString Error;
	if (!bMerge && !GetDefault<UNGInventoryItemDefinition>(State.ItemClass)->ValidateDefinition(Error)) return false;
	if (bSwap && !GetDefault<UNGInventoryItemDefinition>(OtherState.ItemClass)->ValidateDefinition(Error)) return false;
	// Prepare instances under their destination actors before publishing either slot.
	TStrongObjectPtr<UNGInventoryItemInstance> NewTarget(bMerge ? nullptr : Target->InventoryList.CreateInstance(State.ItemClass, &State.StatStacks));
	TStrongObjectPtr<UNGInventoryItemInstance> NewSource(bSwap ? InventoryList.CreateInstance(OtherState.ItemClass, &OtherState.StatStacks) : nullptr);
	if (GetItemAtSlot(SourceSlot) != Item || GetCount(Item) != OldCount
		|| Target->GetItemAtSlot(TargetSlot) != Other || (Other && Target->GetCount(Other) != TargetCount)
		|| (!bMerge && !NewTarget.IsValid()) || (bSwap && !NewSource.IsValid())
		|| !Item->CaptureState(Count).StatStacks.OrderIndependentCompareEqual(State.StatStacks)
		|| (Other && !Other->CaptureState().StatStacks.OrderIndependentCompareEqual(OtherState.StatStacks))) return false;
	auto* SourceEntry = FindSlot(SourceSlot);
	auto* TargetEntry = Target->FindSlot(TargetSlot);
	SourceEntry->Instance = Count == OldCount ? NewSource.Get() : Item;
	SourceEntry->StackCount = bSwap ? TargetCount : OldCount - Count;
	TargetEntry->Instance = bMerge ? Other : NewTarget.Get();
	TargetEntry->StackCount = bMerge ? TargetCount + Count : Count;
	InventoryList.MarkItemDirty(*SourceEntry); Target->InventoryList.MarkItemDirty(*TargetEntry);
	if (IsUsingRegisteredSubObjectList())
	{
		if (Count == OldCount) RemoveReplicatedSubObject(Item);
		if (bSwap && IsReadyForReplication()) AddReplicatedSubObject(NewSource.Get(), bOwnerOnly ? COND_OwnerOnly : COND_None);
	}
	if (Target->IsUsingRegisteredSubObjectList())
	{
		if (bSwap) Target->RemoveReplicatedSubObject(Other);
		if (!bMerge && Target->IsReadyForReplication()) Target->AddReplicatedSubObject(NewTarget.Get(), Target->bOwnerOnly ? COND_OwnerOnly : COND_None);
	}
	NotifyInventoryChanged();
	Target->NotifyInventoryChanged();
	return true;
}
