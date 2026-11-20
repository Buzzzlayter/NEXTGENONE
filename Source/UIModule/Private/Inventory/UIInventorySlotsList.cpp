#include "Inventory/UIInventorySlotsList.h"
#include "Inventory/UIInventorySlotWidget.h"
#include "Components/UniformGridPanel.h"
#include "Components/WrapBox.h"

void UUIInventorySlotsList::SetSlotLayout(const FVector2D& Size, int32 Columns)
{
	SlotSize = Size;
	if (!WrapBox) return;

	Columns = FMath::Max(0, Columns);
	const bool bColumnsChanged = SlotColumns != Columns;
	SlotColumns = Columns;
	if (SlotColumns > 0 && !FixedGrid)
	{
		FixedGrid = NewObject<UUniformGridPanel>(this);
		FixedGrid->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		const FVector2D InnerSlotPadding = WrapBox->GetInnerSlotPadding();
		FixedGrid->SetSlotPadding(FMargin(0.f, 0.f, static_cast<float>(InnerSlotPadding.X), static_cast<float>(InnerSlotPadding.Y)));
		WrapBox->AddChildToWrapBox(FixedGrid);
	}
	else if (SlotColumns == 0 && FixedGrid)
	{
		FixedGrid->RemoveFromParent();
		FixedGrid = nullptr;
	}

	for (int32 Index = 0; Index < InventorySlots.Num(); ++Index)
	{
		auto* SlotWidget = InventorySlots[Index].Get();
		SlotWidget->SetSlotSize(SlotSize);
		if (bColumnsChanged) AddSlotToLayout(SlotWidget, Index);
	}
}

void UUIInventorySlotsList::AddSlotToLayout(UUIInventorySlotWidget* SlotWidget, int32 Index)
{
	if (FixedGrid)
		FixedGrid->AddChildToUniformGrid(SlotWidget, Index / SlotColumns, Index % SlotColumns);
	else
		WrapBox->AddChildToWrapBox(SlotWidget);
}

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
			SlotWidget->SetSlotSize(SlotSize);
			AddSlotToLayout(SlotWidget, Index);
		}
		InventorySlots[Index]->SetData(InSlots[Index], InOperationHandler);
	}
	OnListUpdated();
}
void UUIInventorySlotsList::NativeDestruct()
{
	if (FixedGrid) FixedGrid->ClearChildren();
	else if (WrapBox) WrapBox->ClearChildren();
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
