// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#pragma once

#include "GameplayTagContainer.h"
#include "Inventory/NGInventoryItemDefinition.h"

#include "NGInventoryFragment_SetStats.generated.h"

class UNGInventoryItemInstance;
class UObject;
struct FGameplayTag;

UCLASS()
class GAMEPLAYFOUNDATION_API UNGInventoryFragment_SetStats : public UNGInventoryItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category=Equipment)
	TMap<FGameplayTag, int32> InitialItemStats;

public:
	virtual void OnInstanceCreated(UNGInventoryItemInstance* Instance) const override;

	int32 GetItemStatByTag(FGameplayTag Tag) const;
};
