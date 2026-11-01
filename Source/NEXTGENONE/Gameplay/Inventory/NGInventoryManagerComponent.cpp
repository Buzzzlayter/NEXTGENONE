// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#include "NGInventoryManagerComponent.h"

#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "NGInventoryItemDefinition.h"
#include "NGInventoryItemInstance.h"
#include "NativeGameplayTags.h"
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
		check(Stack.LastObservedCount != INDEX_NONE);
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

	if (auto* Manager = Cast<UNGInventoryManagerComponent>(OwnerComponent)) Manager->OnInventoryChanged.Broadcast(Message);
}

UNGInventoryItemInstance* FNGInventoryList::AddEntry(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount, const TMap<FGameplayTag, int32>* RestoredStats, bool bNotify)
{
	UNGInventoryItemInstance* Result = nullptr;

	check(ItemDef != nullptr);
 	check(OwnerComponent);

	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor->HasAuthority());


	FNGInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = NewObject<UNGInventoryItemInstance>(OwnerComponent->GetOwner());  //@TODO: Using the actor instead of component as the outer due to UE-127172
	NewEntry.Instance->SetItemDef(ItemDef);
	for (UNGInventoryItemFragment* Fragment : GetDefault<UNGInventoryItemDefinition>(ItemDef)->Fragments)
	{
		if (Fragment != nullptr)
		{
			Fragment->OnInstanceCreated(NewEntry.Instance);
		}
	}
	if (RestoredStats) NewEntry.Instance->RestoreStatStacks(*RestoredStats);
	NewEntry.StackCount = StackCount;
	Result = NewEntry.Instance;

	//const UNGInventoryItemDefinition* ItemCDO = GetDefault<UNGInventoryItemDefinition>(ItemDef);
	MarkItemDirty(NewEntry);
	if (bNotify) BroadcastChangeMessage(NewEntry, 0, StackCount);

	return Result;
}

void FNGInventoryList::AddEntry(UNGInventoryItemInstance* Instance)
{
	if (!Instance || !OwnerComponent || !OwnerComponent->GetOwner()->HasAuthority() || Instance->GetOuter() != OwnerComponent->GetOwner()) return;
	for (const auto& Entry : Entries) if (Entry.Instance == Instance) return;
	auto& Entry = Entries.AddDefaulted_GetRef(); Entry.Instance = Instance; Entry.StackCount = 1;
	MarkItemDirty(Entry); BroadcastChangeMessage(Entry, 0, 1);
}

void FNGInventoryList::RemoveEntry(UNGInventoryItemInstance* Instance, bool bNotify)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FNGInventoryEntry& Entry = *EntryIt;
		if (Entry.Instance == Instance)
		{
			FNGInventoryEntry RemovedEntry = Entry;
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
			if (bNotify) BroadcastChangeMessage(RemovedEntry, RemovedEntry.StackCount, 0);
		}
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
}

void UNGInventoryManagerComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, InventoryList, COND_OwnerOnly);
}

bool UNGInventoryManagerComponent::CanAddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount)
{
	return GetOwner() && GetOwner()->HasAuthority() && ItemDef && StackCount > 0
		&& StackCount <= GetDefault<UNGInventoryItemDefinition>(ItemDef)->MaxStackSize && InventoryList.Entries.Num() < MaxEntries;
}

UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount)
{
	UNGInventoryItemInstance* Result = nullptr;
	if (CanAddItemDefinition(ItemDef, StackCount))
	{
		Result = InventoryList.AddEntry(ItemDef, StackCount);
		
		if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && Result)
		{
			AddReplicatedSubObject(Result, COND_OwnerOnly);
		}
	}
	return Result;
}

void UNGInventoryManagerComponent::AddItemInstance(UNGInventoryItemInstance* ItemInstance)
{
	if (!GetOwner()->HasAuthority() || !ItemInstance || ItemInstance->GetOuter() != GetOwner() || Contains(ItemInstance) || InventoryList.Entries.Num() >= MaxEntries) return;
	InventoryList.AddEntry(ItemInstance);
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && ItemInstance)
	{
		AddReplicatedSubObject(ItemInstance, COND_OwnerOnly);
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
				AddReplicatedSubObject(Instance, COND_OwnerOnly);
			}
		}
	}
}

bool UNGInventoryManagerComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	if (!RepFlags->bNetOwner) return WroteSomething;

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

bool UNGInventoryManagerComponent::Contains(UNGInventoryItemInstance* Item) const
{
	return Item && InventoryList.Entries.ContainsByPredicate([Item](const auto& Entry) { return Entry.Instance == Item; });
}
int32 UNGInventoryManagerComponent::GetCount(UNGInventoryItemInstance* Item) const
{
	for (const auto& Entry : InventoryList.Entries) if (Entry.Instance == Item) return Entry.StackCount;
	return 0;
}
UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemState(const FNGItemState& State, bool bNotify)
{
	if (!CanAddItemDefinition(State.ItemClass, State.Count)) return nullptr;
	for (const auto& Pair : State.StatStacks) if (!Pair.Key.IsValid() || Pair.Value < 0) return nullptr;
	auto* Item = InventoryList.AddEntry(State.ItemClass, State.Count, &State.StatStacks, bNotify);
	if (Item && IsUsingRegisteredSubObjectList() && IsReadyForReplication()) AddReplicatedSubObject(Item, COND_OwnerOnly);
	return Item;
}
UNGInventoryItemInstance* UNGInventoryManagerComponent::AddItemDefinitionDeferred(TSubclassOf<UNGInventoryItemDefinition> ItemClass,int32 Count)
{
	if (!CanAddItemDefinition(ItemClass,Count)) return nullptr;
	auto* Item=InventoryList.AddEntry(ItemClass,Count,nullptr,false);
	if (Item && IsUsingRegisteredSubObjectList() && IsReadyForReplication()) AddReplicatedSubObject(Item,COND_OwnerOnly);
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
void UNGInventoryManagerComponent::NotifyItemChange(UNGInventoryItemInstance* Item, int32 OldCount, int32 NewCount)
{
	FNGInventoryChangeMessage Message; Message.InventoryOwner = this; Message.Instance = Item;
	Message.NewCount = NewCount; Message.Delta = NewCount-OldCount; OnInventoryChanged.Broadcast(Message);
}

//////////////////////////////////////////////////////////////////////
//

// UCLASS(Abstract)
// class UNGInventoryFilter : public UObject
// {
// public:
// 	virtual bool PassesFilter(UNGInventoryItemInstance* Instance) const { return true; }
// };

// UCLASS()
// class UNGInventoryFilter_HasTag : public UNGInventoryFilter
// {
// public:
// 	virtual bool PassesFilter(UNGInventoryItemInstance* Instance) const { return true; }
// };


