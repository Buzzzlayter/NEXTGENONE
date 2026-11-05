
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Inventory/UIInventoryTypes.h"
#include "UIInventoryOperationInterface.generated.h"

UINTERFACE(BlueprintType)
class UIMODULE_API UUIInventoryOperationInterface : public UInterface
{
	GENERATED_BODY()
};

class UIMODULE_API IUIInventoryOperationInterface
{
	GENERATED_BODY()
public:
	/** True means the request was dispatched, not that a server operation has succeeded. Amount=0 means all. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory")
	bool RequestInventoryOperation(const FUIInventorySlotData& SlotData, EUIInventoryOperation Operation, int32 Amount);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory")
	bool MoveInventoryItem(const FUIInventorySlotData& Source, const FUIInventorySlotData& Destination, int32 Amount);
};
