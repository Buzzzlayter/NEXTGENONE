#include "Inventory/UIInventorySlotsList.h"
#include "Inventory/UIInventorySlotWidget.h"
#include "Components/WrapBox.h"
#include "GameFramework/PlayerController.h"

void UUIInventorySlotsList::SetData(const TArray<FUIInventorySlotData>& InSlots, UObject* InOperationHandler)
{
	Slots = InSlots;
	OperationHandler = InOperationHandler;
	Repaint();
}
void UUIInventorySlotsList::NativeConstruct() { Super::NativeConstruct(); Repaint(); }
void UUIInventorySlotsList::NativeDestruct()
{
	if (WrapBox) WrapBox->ClearChildren();
	InventorySlots.Reset();
	Super::NativeDestruct();
}
void UUIInventorySlotsList::Repaint()
{
	if (!WrapBox || !SlotClass || SlotClass->HasAnyClassFlags(CLASS_Abstract) || !GetOwningPlayer()) return;
	while (InventorySlots.Num() > Slots.Num())
	{
		InventorySlots.Last()->RemoveFromParent();
		InventorySlots.Pop();
	}
	while (InventorySlots.Num() < Slots.Num())
	{
		auto* SlotWidget = CreateWidget<UUIInventorySlotWidget>(GetOwningPlayer(), SlotClass);
		if (!SlotWidget) return;
		InventorySlots.Add(SlotWidget);
		WrapBox->AddChildToWrapBox(SlotWidget);
	}
	for (int32 Index = 0; Index < Slots.Num(); ++Index) InventorySlots[Index]->SetData(Slots[Index], OperationHandler.Get());
	OnListUpdated();
}
UUIInventorySlotWidget* UUIInventorySlotsList::GetSlotWidget(int32 Index) const { return InventorySlots.IsValidIndex(Index) ? InventorySlots[Index].Get() : nullptr; }
void UUIInventorySlotsList::SetFirstSlotFocused() { if (auto* SlotWidget = GetSlotWidget(0)) SlotWidget->FocusSlot(); }
void UUIInventorySlotsList::SetWrapSize(float Size) { if (WrapBox) WrapBox->SetWrapSize(Size); }
void UUIInventorySlotsList::SetInnerSlotPadding(FVector2D SlotPadding) { if (WrapBox) WrapBox->SetInnerSlotPadding(SlotPadding); }
