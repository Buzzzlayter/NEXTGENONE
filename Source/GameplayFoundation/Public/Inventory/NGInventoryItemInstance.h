// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#pragma once

#include "Inventory/NGGameplayTagStack.h"
#include "Templates/SubclassOf.h"

#include "NGInventoryItemInstance.generated.h"



class UNGInventoryItemDefinition;
class UNGInventoryItemFragment;
struct FFrame;
struct FGameplayTag;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNGItemStatsChanged);

USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGItemState
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<UNGInventoryItemDefinition> ItemClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FGameplayTag, int32> StatStacks;
};

/**
 * UNGInventoryItemInstance
 */
UCLASS(BlueprintType)
class GAMEPLAYFOUNDATION_API UNGInventoryItemInstance : public UObject
{
	GENERATED_BODY()

public:
	UNGInventoryItemInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void PostInitProperties() override;
	UFUNCTION(BlueprintPure, Category=Inventory) FNGItemState CaptureState(int32 Count = 1) const;
	UPROPERTY(BlueprintAssignable, Category=Inventory) FNGItemStatsChanged OnStatsChanged;

	//~UObject interface
	virtual bool IsSupportedForNetworking() const override { return true; }
	//~End of UObject interface

	// Adds a specified number of stacks to the tag (does nothing if StackCount is below 1)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void AddStatTagStack(FGameplayTag Tag, int32 StackCount);

	// Removes a specified number of stacks from the tag (does nothing if StackCount is below 1)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category= Inventory)
	void RemoveStatTagStack(FGameplayTag Tag, int32 StackCount);

	// Returns the stack count of the specified tag (or 0 if the tag is not present)
	UFUNCTION(BlueprintCallable, Category=Inventory)
	int32 GetStatTagStackCount(FGameplayTag Tag) const;

	// Returns true if there is at least one stack of the specified tag
	UFUNCTION(BlueprintCallable, Category=Inventory)
	bool HasStatTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category=Inventory) TSubclassOf<UNGInventoryItemDefinition> GetItemDef() const
	{
		return ItemDef;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure=false, meta=(DeterminesOutputType=FragmentClass))
	const UNGInventoryItemFragment* FindFragmentByClass(TSubclassOf<UNGInventoryItemFragment> FragmentClass) const;

	template <typename ResultClass>
	const ResultClass* FindFragmentByClass() const
	{
		return (ResultClass*)FindFragmentByClass(ResultClass::StaticClass());
	}

private:


	void SetItemDef(TSubclassOf<UNGInventoryItemDefinition> InDef);
	void RestoreStatStacks(const TMap<FGameplayTag, int32>& Counts);

	friend struct FNGInventoryList;

private:
	UPROPERTY(Replicated)
	FNGGameplayTagStackContainer StatTags;

	// The item definition
	UPROPERTY(ReplicatedUsing=OnRep_ItemDef)
	TSubclassOf<UNGInventoryItemDefinition> ItemDef;
	UFUNCTION() void OnRep_ItemDef();
};
