#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UIInventoryTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EUIInventoryOperation : uint8 { PrimaryAction, SecondaryAction, Drop };

/** Presentation snapshot, not inventory authority. Item/Inventory identify the backend objects. */
USTRUCT(BlueprintType)
struct UIMODULE_API FUIInventorySlotData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UObject> Inventory;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UObject> Item;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SlotIndex = INDEX_NONE;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag SlotTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StackLimit = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEquipped = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer ItemTags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FGameplayTag, int32> StatStacks;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EUIInventoryOperation> AvailableOperations;
};
