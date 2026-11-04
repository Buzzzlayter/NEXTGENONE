#include "Inventory/UIInventoryWidget.h"
#include "Inventory/UIInventorySlotsList.h"

void UUIInventoryWidget::SetInventoryData(FText Name, const TArray<FUIInventorySlotData>& InSlots, UObject* Handler)
{
	InventoryName = Name;
	SlotData = InSlots;
	OperationHandler = Handler;
	if (SlotsList) SlotsList->SetData(SlotData, Handler);
	OnInventoryUpdated();
}
void UUIInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (SlotsList) SlotsList->SetData(SlotData, OperationHandler.Get());
}
