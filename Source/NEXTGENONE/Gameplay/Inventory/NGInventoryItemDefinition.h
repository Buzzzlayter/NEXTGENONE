// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "NGInventoryItemDefinition.generated.h"

template <typename T> class TSubclassOf;

class UNGInventoryItemInstance;
class UNGWeaponDefinition;
class UStaticMesh;
class USkeletalMesh;
struct FFrame;

//////////////////////////////////////////////////////////////////////

// Represents a fragment of an item definition
UCLASS(DefaultToInstanced, EditInlineNew, Abstract)
class NEXTGENONE_API UNGInventoryItemFragment : public UObject
{
	GENERATED_BODY()

public:
	virtual void OnInstanceCreated(UNGInventoryItemInstance* Instance) const {}
};

//////////////////////////////////////////////////////////////////////

/**
 * UNGInventoryItemDefinition
 */
UCLASS(Blueprintable, Const, Abstract)
class UNGInventoryItemDefinition : public UObject
{
	GENERATED_BODY()

public:
	UNGInventoryItemDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display)
	FText DisplayName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Equipment) TObjectPtr<UNGWeaponDefinition> Weapon;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Inventory, meta=(ClampMin="1")) int32 MaxStackSize = 100;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World Pickup") TObjectPtr<UStaticMesh> PickupMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World Pickup") TObjectPtr<USkeletalMesh> PickupSkeletalMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World Pickup") FTransform PickupTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display, Instanced)
	TArray<TObjectPtr<UNGInventoryItemFragment>> Fragments;

public:
	const UNGInventoryItemFragment* FindFragmentByClass(TSubclassOf<UNGInventoryItemFragment> FragmentClass) const;
};

//@TODO: Make into a subsystem instead?
UCLASS()
class UNGInventoryFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, meta=(DeterminesOutputType=FragmentClass))
	static const UNGInventoryItemFragment* FindItemDefinitionFragment(TSubclassOf<UNGInventoryItemDefinition> ItemDef, TSubclassOf<UNGInventoryItemFragment> FragmentClass);
};
