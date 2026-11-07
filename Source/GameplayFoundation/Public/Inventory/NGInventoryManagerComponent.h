// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#pragma once

#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "NativeGameplayTags.h"

#include "NGInventoryManagerComponent.generated.h"

class UNGInventoryItemDefinition;
class UNGInventoryItemInstance;
class UNGInventoryManagerComponent;
class UNGInventoryLayoutDefinition;
class UObject;
struct FFrame;
struct FNGInventoryList;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;

GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGSlotStorage);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGSlotAction);

USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGInventorySlotState
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SlotIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FNGItemState Item;
};

/** A message when an item is added to the inventory */
USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGInventoryChangeMessage
{
	GENERATED_BODY()

	//@TODO: Tag based names+owning actors for inventories instead of directly exposing the component?
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	TObjectPtr<UNGInventoryItemInstance> Instance = nullptr;

	UPROPERTY(BlueprintReadOnly, Category=Inventory) int32 NewCount = 0;
	UPROPERTY(BlueprintReadOnly, Category=Inventory) int32 Delta = 0;
	UPROPERTY(BlueprintReadOnly, Category=Inventory) int32 SlotIndex = INDEX_NONE;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGInventoryChanged, FNGInventoryChangeMessage, Change);

/** One persistent slot: rules and optional contents share one replicated record. */
USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FNGInventoryEntry()
	{}

	UPROPERTY() FGameplayTag SlotTag;
	UPROPERTY() FGameplayTagQuery AcceptedItems;
	FString GetDebugString() const;

private:
	friend FNGInventoryList;
	friend UNGInventoryManagerComponent;

	UPROPERTY()
	TObjectPtr<UNGInventoryItemInstance> Instance = nullptr;

	UPROPERTY()
	int32 StackCount = 0;
	UPROPERTY() int32 SlotIndex = INDEX_NONE;

	UPROPERTY(NotReplicated)
	int32 LastObservedCount = INDEX_NONE;
};

/** Single replicated array of persistent slots, including empty ones. */
USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	FNGInventoryList()
		: OwnerComponent(nullptr)
	{
	}

	FNGInventoryList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	TArray<UNGInventoryItemInstance*> GetAllItems() const;

public:
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FNGInventoryEntry, FNGInventoryList>(Entries, DeltaParms, *this);
	}

	UNGInventoryItemInstance* AddEntry(TSubclassOf<UNGInventoryItemDefinition> ItemClass, int32 StackCount, const TMap<FGameplayTag, int32>* RestoredStats=nullptr, bool bNotify=true, int32 SlotIndex=INDEX_NONE);
	void AddEntry(UNGInventoryItemInstance* Instance);

	void RemoveEntry(UNGInventoryItemInstance* Instance, bool bNotify=true);

private:
	void BroadcastChangeMessage(FNGInventoryEntry& Entry, int32 OldCount, int32 NewCount);

private:
	friend UNGInventoryManagerComponent;

private:
	// Runtime slots, including empty ones. Authoring rules belong to Layout.
	UPROPERTY()
	TArray<FNGInventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FNGInventoryList> : public TStructOpsTypeTraitsBase2<FNGInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};










/**
 * Manages an inventory
 */
UCLASS(BlueprintType, ClassGroup=Inventory, meta=(BlueprintSpawnableComponent))
class GAMEPLAYFOUNDATION_API UNGInventoryManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNGInventoryManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	UPROPERTY() FNGInventoryChanged OnInventoryChanged;
	/** Fixed per component template; do not change the visibility policy during replication. */
	UPROPERTY(EditDefaultsOnly, Category="Inventory|Replication") bool bOwnerOnly = true;
	/** Idempotent server initialization; an assigned layout may intentionally contain zero slots. */
	bool InitializeInventory();
	virtual void InitializeComponent() override;
	virtual void GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const override;
	int32 GetSlotCount() const { return InventoryList.Entries.Num(); }
	UNGInventoryItemInstance* GetItemAtSlot(int32 SlotIndex) const;
	int32 GetSlotIndex(UNGInventoryItemInstance* Item) const;
	int32 FindFirstEmptySlot() const;
	int32 FindFirstAvailableSlot(TSubclassOf<UNGInventoryItemDefinition> ItemClass, int32 StackCount = 1, FGameplayTag SlotTag = {}) const;
	FGameplayTag GetSlotTag(int32 SlotIndex) const;
	int32 FindSlotByTag(FGameplayTag SlotTag, int32 Ordinal) const;
	bool CanPlaceItemAtSlot(TSubclassOf<UNGInventoryItemDefinition> ItemClass, int32 SlotIndex) const;
	bool MoveItemToSlot(UNGInventoryItemInstance* Item, int32 SlotIndex);
	UNGInventoryItemInstance* ImportItemState(const FNGItemState& State, int32 SlotIndex);
	TArray<FNGInventorySlotState> CaptureInventoryState() const;
	/** Trusted server primitive; caller validates player access/distance. Amount=0 transfers the whole stack. */
	bool TransferTo(UNGInventoryManagerComponent* Target, int32 SourceSlot, int32 TargetSlot, int32 Amount=0);
	bool Contains(UNGInventoryItemInstance* Item) const;
	/** All currently received entries have usable item subobjects; empty is valid. Not a backend completion marker. */
	bool HasResolvedItems() const;
	int32 GetCount(UNGInventoryItemInstance* Item) const;
	// Internal transfer primitives: subsystem publishes notifications after both owners have committed.
	UNGInventoryItemInstance* AddItemState(const FNGItemState& State, bool bNotify=true, int32 SlotIndex=INDEX_NONE);
	UNGInventoryItemInstance* AddItemDefinitionDeferred(TSubclassOf<UNGInventoryItemDefinition> ItemClass, int32 Count, int32 SlotIndex=INDEX_NONE);
	bool RemoveQuantity(UNGInventoryItemInstance* Item, int32 Count, bool bNotify=true);
	void NotifyItemChange(UNGInventoryItemInstance* Item, int32 OldCount, int32 NewCount, int32 SlotIndex=INDEX_NONE);

	bool CanAddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount = 1) const;

	UNGInventoryItemInstance* AddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount = 1);
	UNGInventoryItemInstance* AddItemDefinitionToSlot(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 SlotIndex, int32 StackCount = 1);

	void AddItemInstance(UNGInventoryItemInstance* ItemInstance);

	void RemoveItemInstance(UNGInventoryItemInstance* ItemInstance);

	TArray<UNGInventoryItemInstance*> GetAllItems() const;

	UNGInventoryItemInstance* FindFirstItemStackByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef) const;

	int32 GetTotalItemCountByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef) const;
	bool ConsumeItemsByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 NumToConsume);
	/** Trusted synchronous credit after all debits, before inventory notifications. */
	bool ConsumeItemsByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 NumToConsume, TFunctionRef<void()> OnConsumed);

	//~UObject interface
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;
	//~End of UObject interface

private:
	friend struct FNGInventoryList;
	struct FMutationScope;
	UNGInventoryItemInstance* AddItemInternal(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 Count, const TMap<FGameplayTag, int32>* Stats, bool bNotify, int32 SlotIndex);
	bool RemoveQuantityInternal(UNGInventoryItemInstance* Item, int32 Count, bool bNotify);
	void QueueChange(const FNGInventoryChangeMessage& Message);
	void PublishChanges();
	TMap<int32, FNGInventoryChangeMessage> PendingChanges;
	bool bTransferring = false;
	bool bSlotsInitialized = false;
	FNGInventoryEntry* FindSlot(int32 SlotIndex);
	const FNGInventoryEntry* FindSlot(int32 SlotIndex) const;
	UPROPERTY(EditDefaultsOnly, Category=Inventory)
	TObjectPtr<UNGInventoryLayoutDefinition> Layout;
	UPROPERTY(Transient, Replicated)
	FNGInventoryList InventoryList;
};
