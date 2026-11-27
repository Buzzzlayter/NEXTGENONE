#include "Inventory/UIInventorySlotWidget.h"
#include "Inventory/UIInventoryOperationInterface.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SWidget.h"

FReply UUIInventorySlotWidget::HandleQuickTransfer(const FGeometry&, const FPointerEvent& Event)
{
	return Event.GetEffectingButton() == EKeys::LeftMouseButton && RequestOperation(EUIInventoryOperation::QuickTransfer)
		? FReply::Handled() : FReply::Unhandled();
}

FReply UUIInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FReply Reply = HandleQuickTransfer(Geometry, Event);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnMouseButtonDoubleClick(Geometry, Event);
}

void UUIInventorySlotWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	Super::NativeOnDragDetected(Geometry, Event, Operation);
	if (!Operation) DragDetected.ExecuteIfBound(Geometry, Event, Operation);
}

void UUIInventorySlotWidget::SetItemDragHidden(bool bHidden)
{
	bItemDragHidden = bHidden;
	UpdateItemVisibility();
}

void UUIInventorySlotWidget::SetSlotSize(const FVector2D& Size)
{
	if (!SlotSizeBox) return;
	if (Size.X > 0.0) SlotSizeBox->SetWidthOverride(static_cast<float>(Size.X));
	if (Size.Y > 0.0) SlotSizeBox->SetHeightOverride(static_cast<float>(Size.Y));
}

void UUIInventorySlotWidget::SetData(const FUIInventorySlotData& InData, UObject* InOperationHandler)
{
	if (OperationHandler.Get() == InOperationHandler && Data.HasSamePresentation(InData)) return;
	const bool bIconChanged = Data.Item != InData.Item || Data.Icon != InData.Icon || (Data.Count > 0) != (InData.Count > 0);
	bItemDragHidden = false;
	Data = InData;
	OperationHandler = InOperationHandler;
	if (bIconChanged) UpdateItemIcon();
	OnSlotUpdated();
	UpdateItemText();
	UpdateItemVisibility();
}

void UUIInventorySlotWidget::UpdateItemText()
{
	const bool bHasItem = ContainsItem();
	if (txt_name) txt_name->SetText(bHasItem ? Data.Name : FText::GetEmpty());
	if (txt_count) txt_count->SetText(bHasItem ? FText::AsNumber(Data.Count) : FText::GetEmpty());
}

void UUIInventorySlotWidget::UpdateItemVisibility()
{
	const bool bHasItem = ContainsItem();
	const bool bShowItem = bHasItem && !bItemDragHidden;
	if (img_item) img_item->SetVisibility(bShowItem && !Data.Icon.IsNull() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (txt_name) txt_name->SetVisibility(bShowItem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (txt_count) txt_count->SetVisibility(bShowItem && Data.Count > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UUIInventorySlotWidget::UpdateItemIcon()
{
	if (!img_item) return;
	const bool bHasIcon = ContainsItem() && !Data.Icon.IsNull();
	bHasIcon ? img_item->SetBrushFromSoftTexture(Data.Icon) : img_item->SetBrushFromTexture(nullptr);
}

bool UUIInventorySlotWidget::RequestOperation(EUIInventoryOperation Operation, int32 Amount)
{
	UObject* Handler = OperationHandler.Get();
	if (!ContainsItem() || bItemDragHidden || Amount < 0 || Amount > Data.Count
		|| !Handler || !Handler->GetClass()->ImplementsInterface(UUIInventoryOperationInterface::StaticClass())) return false;
	return IUIInventoryOperationInterface::Execute_RequestInventoryOperation(Handler, Data, Operation, Amount);
}

void UUIInventorySlotWidget::PrimaryAction()
{
	RequestOperation(EUIInventoryOperation::PrimaryAction);
}
bool UUIInventorySlotWidget::MoveItemHere(const FUIInventorySlotData& Source, int32 Amount)
{
	UObject* Handler = OperationHandler.Get();
	if (!Handler || !Handler->GetClass()->ImplementsInterface(UUIInventoryOperationInterface::StaticClass())
		|| !IsValid(Source.Item) || Amount < 0 || Amount > Source.Count || Data.SlotIndex < 0) return false;
	return IUIInventoryOperationInterface::Execute_MoveInventoryItem(Handler, Source, Data, Amount);
}
void UUIInventorySlotWidget::FocusSlot() { if (btn_Root && GetOwningPlayer()) btn_Root->SetUserFocus(GetOwningPlayer()); }

void UUIInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateItemIcon();
	UpdateItemText();
	UpdateItemVisibility();
	if (btn_Root)
	{
		if (const auto ButtonWidget = btn_Root->GetCachedWidget())
			ButtonWidget->SetOnMouseDoubleClick(FPointerEventHandler::CreateUObject(this, &ThisClass::HandleQuickTransfer));
		btn_Root->OnClicked.AddUniqueDynamic(this, &ThisClass::PrimaryAction);
		btn_Root->OnPressed.AddUniqueDynamic(this, &ThisClass::OnActionPressed);
		btn_Root->OnReleased.AddUniqueDynamic(this, &ThisClass::OnActionReleased);
		btn_Root->OnHovered.AddUniqueDynamic(this, &ThisClass::OnActionHover);
		btn_Root->OnUnhovered.AddUniqueDynamic(this, &ThisClass::OnActionUnhover);
	}
}

void UUIInventorySlotWidget::NativeDestruct()
{
	bItemDragHidden = false;
	if (btn_Root)
	{
		if (const auto ButtonWidget = btn_Root->GetCachedWidget())
			ButtonWidget->SetOnMouseDoubleClick({});
		btn_Root->OnClicked.RemoveDynamic(this, &ThisClass::PrimaryAction);
		btn_Root->OnPressed.RemoveDynamic(this, &ThisClass::OnActionPressed);
		btn_Root->OnReleased.RemoveDynamic(this, &ThisClass::OnActionReleased);
		btn_Root->OnHovered.RemoveDynamic(this, &ThisClass::OnActionHover);
		btn_Root->OnUnhovered.RemoveDynamic(this, &ThisClass::OnActionUnhover);
	}
	Super::NativeDestruct();
}
