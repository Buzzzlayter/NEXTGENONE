// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#pragma once

#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "NGInventoryItemInstance.h"

#include "NGInventoryManagerComponent.generated.h"

class UNGInventoryItemDefinition;
class UNGInventoryItemInstance;
class UNGInventoryManagerComponent;
class UObject;
struct FFrame;
struct FNGInventoryList;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;

/** A message when an item is added to the inventory */
USTRUCT(BlueprintType)
struct FNGInventoryChangeMessage
{
	GENERATED_BODY()

	//@TODO: Tag based names+owning actors for inventories instead of directly exposing the component?
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	TObjectPtr<UNGInventoryItemInstance> Instance = nullptr;

	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 NewCount = 0;

	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 Delta = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGInventoryChanged, FNGInventoryChangeMessage, Change);

/** A single entry in an inventory */
USTRUCT(BlueprintType)
struct FNGInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FNGInventoryEntry()
	{}

	FString GetDebugString() const;

private:
	friend FNGInventoryList;
	friend UNGInventoryManagerComponent;

	UPROPERTY()
	TObjectPtr<UNGInventoryItemInstance> Instance = nullptr;

	UPROPERTY()
	int32 StackCount = 0;

	UPROPERTY(NotReplicated)
	int32 LastObservedCount = INDEX_NONE;
};

/** List of inventory items */
USTRUCT(BlueprintType)
struct FNGInventoryList : public FFastArraySerializer
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

	UNGInventoryItemInstance* AddEntry(TSubclassOf<UNGInventoryItemDefinition> ItemClass, int32 StackCount, const TMap<FGameplayTag, int32>* RestoredStats=nullptr, bool bNotify=true);
	void AddEntry(UNGInventoryItemInstance* Instance);

	void RemoveEntry(UNGInventoryItemInstance* Instance, bool bNotify=true);

private:
	void BroadcastChangeMessage(FNGInventoryEntry& Entry, int32 OldCount, int32 NewCount);

private:
	friend UNGInventoryManagerComponent;

private:
	// Replicated list of items
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
UCLASS(BlueprintType)
class NEXTGENONE_API UNGInventoryManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNGInventoryManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	UPROPERTY(BlueprintAssignable, Category=Inventory) FNGInventoryChanged OnInventoryChanged;
	UPROPERTY(EditDefaultsOnly, Category=Inventory, meta=(ClampMin="1", ClampMax="256")) int32 MaxEntries = 64;
	UFUNCTION(BlueprintPure, Category=Inventory) bool Contains(UNGInventoryItemInstance* Item) const;
	UFUNCTION(BlueprintPure, Category=Inventory) int32 GetCount(UNGInventoryItemInstance* Item) const;
	// Internal transfer primitives: subsystem publishes notifications after both owners have committed.
	UNGInventoryItemInstance* AddItemState(const FNGItemState& State, bool bNotify=true);
	UNGInventoryItemInstance* AddItemDefinitionDeferred(TSubclassOf<UNGInventoryItemDefinition> ItemClass, int32 Count);
	bool RemoveQuantity(UNGInventoryItemInstance* Item, int32 Count, bool bNotify=true);
	void NotifyItemChange(UNGInventoryItemInstance* Item, int32 OldCount, int32 NewCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	bool CanAddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	UNGInventoryItemInstance* AddItemDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void AddItemInstance(UNGInventoryItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void RemoveItemInstance(UNGInventoryItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure=false)
	TArray<UNGInventoryItemInstance*> GetAllItems() const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	UNGInventoryItemInstance* FindFirstItemStackByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef) const;

	int32 GetTotalItemCountByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef) const;
	bool ConsumeItemsByDefinition(TSubclassOf<UNGInventoryItemDefinition> ItemDef, int32 NumToConsume);

	//~UObject interface
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;
	//~End of UObject interface

private:
	UPROPERTY(Replicated)
	FNGInventoryList InventoryList;
};
