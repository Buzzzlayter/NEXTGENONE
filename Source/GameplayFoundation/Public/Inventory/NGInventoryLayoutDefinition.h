#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "NGInventoryLayoutDefinition.generated.h"

/** Authoring rules only; no item instances or replicated state. Array order defines slot indices. */
USTRUCT()
struct GAMEPLAYFOUNDATION_API FNGInventorySlotDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FGameplayTag SlotTag;
	UPROPERTY(EditAnywhere) FGameplayTagQuery AcceptedItems;
};

/** Shared initial layout for a player inventory, chest, or bag. */
UCLASS(BlueprintType)
class GAMEPLAYFOUNDATION_API UNGInventoryLayoutDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Inventory", meta=(TitleProperty="SlotTag"))
	TArray<FNGInventorySlotDefinition> Slots;
};
