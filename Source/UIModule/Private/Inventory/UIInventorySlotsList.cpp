#include "Inventory/UIInventorySlotsList.h"
#include "Inventory/UIInventorySlotWidget.h"
#include "Components/WrapBox.h"

void UUIInventorySlotsList::SetData(const TArray<FUIInventorySlotData>& InSlots, UObject* InOperationHandler)
{
	if (!WrapBox) return;
	const int32 NewCount = InSlots.Num();
	const int32 OldCount = InventorySlots.Num();

	if (NewCount < OldCount)
	{
		for (int32 Index = NewCount; Index < OldCount; ++Index) InventorySlots[Index]->RemoveFromParent();
		InventorySlots.SetNum(NewCount, EAllowShrinking::No);
	}
	for (int32 Index = 0; Index < NewCount; ++Index)
	{
		if (Index >= InventorySlots.Num())
		{
			auto* SlotWidget = CreateWidget<UUIInventorySlotWidget>(GetOwningPlayer(), SlotClass);
			if (!SlotWidget) return;
			InventorySlots.Add(SlotWidget);
			WrapBox->AddChildToWrapBox(SlotWidget);
		}
		InventorySlots[Index]->SetData(InSlots[Index], InOperationHandler);
	}
	OnListUpdated();
}
void UUIInventorySlotsList::NativeDestruct()
{
	if (WrapBox) WrapBox->ClearChildren();
	InventorySlots.Reset();
	Super::NativeDestruct();
}

UUIInventorySlotWidget* UUIInventorySlotsList::GetSlotWidget(int32 Index) const { return InventorySlots.IsValidIndex(Index) ? InventorySlots[Index].Get() : nullptr; }

UUIInventorySlotWidget* UUIInventorySlotsList::FindSlotAt(const FVector2D& ScreenPosition) const
{
	if (!GetCachedGeometry().IsUnderLocation(ScreenPosition)) return nullptr;
	for (const auto& Entry : InventorySlots)
	{
		auto* SlotWidget = Entry.Get();
		if (SlotWidget && SlotWidget->IsVisible() && SlotWidget->GetIsEnabled()
			&& SlotWidget->GetCachedGeometry().IsUnderLocation(ScreenPosition)) return SlotWidget;
	}
	return nullptr;
}
