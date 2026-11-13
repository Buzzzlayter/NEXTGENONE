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

/** Starting item: placed in the first compatible free slot, optionally within a tagged group. */
USTRUCT()
struct GAMEPLAYFOUNDATION_API FNGInventoryStarterEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) TSubclassOf<UNGInventoryItemDefinition> ItemClass;
	UPROPERTY(EditAnywhere, meta=(ClampMin="1")) int32 Count = 1;
	UPROPERTY(EditAnywhere) FGameplayTag SlotTag;
};

USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGInventorySlotState
{
	GENERATED_BODY()
	// Runtime restore/capture position, not an authoring setting for InitialContents.
	UPROPERTY(BlueprintReadWrite) int32 SlotIndex = INDEX_NONE;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FNGItemState Item;
};

/** Legacy serialized type targeted by Core Redirects. Not used by runtime notifications. */
USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGInventoryChangeMessage
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category=Inventory) TObjectPtr<UActorComponent> InventoryOwner = nullptr;
	UPROPERTY(BlueprintReadOnly, Category=Inventory) TObjectPtr<UNGInventoryItemInstance> Instance = nullptr;
	UPROPERTY(BlueprintReadOnly, Category=Inventory) int32 NewCount = 0;
	UPROPERTY(BlueprintReadOnly, Category=Inventory) int32 Delta = 0;
	UPROPERTY(BlueprintReadOnly, Category=Inventory) int32 SlotIndex = INDEX_NONE;
};

/** Final inventory invalidation. Observers read a fresh snapshot; no per-slot delta is promised. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNGInventoryChanged);

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
	friend UNGInventoryManagerComponent;
	UNGInventoryItemInstance* CreateInstance(TSubclassOf<UNGInventoryItemDefinition> ItemDef, const TMap<FGameplayTag, int32>* RestoredStats);

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
	UNGInventoryItemInstance* ImportItemState(const FNGItemState& State, int32 SlotIndex=INDEX_NONE);
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
	/** Debit one owned instance and execute a trusted synchronous credit before publishing. */
	bool ConsumeItem(UNGInventoryItemInstance* Item, int32 Count, TFunctionRef<void()> OnConsumed);
	void NotifyInventoryChanged();

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
	void PublishChanges();
	bool bInventoryDirty = false;
	bool bTransferring = false;
	bool bSlotsInitialized = false;
	FNGInventoryEntry* FindSlot(int32 SlotIndex);
	const FNGInventoryEntry* FindSlot(int32 SlotIndex) const;
	UPROPERTY(EditDefaultsOnly, Category=Inventory)
	TObjectPtr<UNGInventoryLayoutDefinition> Layout;
	UPROPERTY(Transient, Replicated)
	FNGInventoryList InventoryList;
};
