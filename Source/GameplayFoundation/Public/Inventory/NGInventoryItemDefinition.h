#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "NGInventoryItemDefinition.generated.h"

template <typename T> class TSubclassOf;

class UNGInventoryItemInstance;
class UTexture2D;
class UStaticMesh;
class USkeletalMesh;
struct FFrame;

//////////////////////////////////////////////////////////////////////

// Represents a fragment of an item definition
UCLASS(DefaultToInstanced, EditInlineNew, Abstract)
class GAMEPLAYFOUNDATION_API UNGInventoryItemFragment : public UObject
{
	GENERATED_BODY()

public:
	virtual void OnInstanceCreated(UNGInventoryItemInstance* Instance) const {}
};

/** Optional equipment payload. Inventory does not know how an equipment asset is used. */
UCLASS(DisplayName="Equipment", DefaultToInstanced, EditInlineNew)
class GAMEPLAYFOUNDATION_API UNGInventoryFragment_Equipment : public UNGInventoryItemFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Equipment) TObjectPtr<UDataAsset> Definition;
};

//////////////////////////////////////////////////////////////////////

/**
 * UNGInventoryItemDefinition
 */
UCLASS(Blueprintable, Const, Abstract)
class GAMEPLAYFOUNDATION_API UNGInventoryItemDefinition : public UObject
{
	GENERATED_BODY()

public:
	UNGInventoryItemDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	bool ValidateDefinition(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	UFUNCTION(BlueprintPure, Category=Inventory) UDataAsset* GetEquipmentDefinition() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display)
	FText DisplayName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display) TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Inventory) FGameplayTagContainer ItemTags;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Inventory, meta=(ClampMin="1")) int32 MaxStackSize = 100;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World Pickup") TObjectPtr<UStaticMesh> PickupMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World Pickup") TObjectPtr<USkeletalMesh> PickupSkeletalMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World Pickup") FTransform PickupTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display, Instanced)
	TArray<TObjectPtr<UNGInventoryItemFragment>> Fragments;

public:
	const UNGInventoryItemFragment* FindFragmentByClass(TSubclassOf<UNGInventoryItemFragment> FragmentClass) const;
};

UCLASS()
class GAMEPLAYFOUNDATION_API UNGInventoryFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, meta=(DeterminesOutputType=FragmentClass))
	static const UNGInventoryItemFragment* FindItemDefinitionFragment(TSubclassOf<UNGInventoryItemDefinition> ItemDef, TSubclassOf<UNGInventoryItemFragment> FragmentClass);
};
