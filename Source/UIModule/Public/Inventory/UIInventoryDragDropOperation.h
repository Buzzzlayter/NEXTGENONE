#pragma once

#include "Blueprint/DragDropOperation.h"
#include "Inventory/UIInventoryTypes.h"
#include "UIInventoryDragDropOperation.generated.h"

class ULocalPlayer;

/** Immutable source snapshot for a single drag. No inventory mutation or network logic. */
UCLASS()
class UIMODULE_API UUIInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()
private:
	friend class UUIInventorySlotWidget;
	UPROPERTY(Transient) FUIInventorySlotData SourceData;
	TWeakObjectPtr<ULocalPlayer> SourcePlayer;
};
