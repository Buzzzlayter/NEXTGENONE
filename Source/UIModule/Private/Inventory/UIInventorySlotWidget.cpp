#include "Inventory/UIInventorySlotWidget.h"
#include "Inventory/UIInventoryOperationInterface.h"
#include "Components/Button.h"
#include "Input/PlayerInputSubsystem.h"
#include "Inventory/UIInventoryDragDropOperation.h"
#include "Components/Image.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

FReply UUIInventorySlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply BlueprintReply = Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	if (BlueprintReply.IsEventHandled() || InMouseEvent.GetEffectingButton() != DragButton || !ContainsItem()
		|| !OperationHandler.IsValid() || !GetOwningLocalPlayer()) return BlueprintReply;
	ReleaseDragPress();
	PointerDownData = Data;
	bDragPending = true;
	OnActionPressed();
	// Preview handles nested Buttons; capture also preserves a normal click when no drag begins.
	const auto SlateWidget = TakeWidget();
	return FReply::Handled().CaptureMouse(SlateWidget).DetectDrag(SlateWidget, DragButton);
}

void UUIInventorySlotWidget::ReleaseDragPress()
{
	const bool bWasPressed = bDragPending;
	bDragPending = false;
	PointerDownData = {};
	if (bWasPressed) OnActionReleased();
}

FReply UUIInventorySlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragPending || InMouseEvent.GetEffectingButton() != DragButton)
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	const auto ClickedData = PointerDownData;
	const bool bClick = InGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
	ReleaseDragPress();
	if (bClick && Data.Item == ClickedData.Item && Data.Inventory == ClickedData.Inventory && Data.SlotIndex == ClickedData.SlotIndex)
		PrimaryAction();
	return FReply::Handled().ReleaseMouseCapture();
}

void UUIInventorySlotWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	ReleaseDragPress();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UUIInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (OutOperation) { ReleaseDragPress(); return; }
	const auto Snapshot = PointerDownData;
	const bool bCanDrag = bDragPending && GetOwningLocalPlayer() && OperationHandler.IsValid()
		&& ContainsItem() && Data.Item == Snapshot.Item
		&& Data.Inventory == Snapshot.Inventory && Data.SlotIndex == Snapshot.SlotIndex;
	ReleaseDragPress();
	if (!bCanDrag) return;
	auto* Drag = NewObject<UUIInventoryDragDropOperation>(this);
	Drag->SourceData = Snapshot;
	Drag->SourcePlayer = GetOwningLocalPlayer();
	Drag->Pivot = EDragPivot::CenterCenter;
	auto* Visual = NewObject<UImage>(Drag);
	Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
	Visual->SetBrushFromSoftTexture(Snapshot.Icon);
	Visual->SetDesiredSizeOverride(InGeometry.GetLocalSize());
	Drag->DefaultDragVisual = Visual;
	OutOperation = Drag;
}

bool UUIInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const auto* Drag = Cast<UUIInventoryDragDropOperation>(InOperation);
	if (!Drag) return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	if (!Drag->SourcePlayer.IsValid() || Drag->SourcePlayer.Get() != GetOwningLocalPlayer()) return false;
	if (Data.Inventory == Drag->SourceData.Inventory && Data.SlotIndex == Drag->SourceData.SlotIndex) return true;
	return MoveItemHere(Drag->SourceData, 0); // Whole stack; server validates and replication refreshes both panels.
}

void UUIInventorySlotWidget::SetData(const FUIInventorySlotData& InData, UObject* InOperationHandler)
{
	Data = InData;
	OperationHandler = InOperationHandler;
	UpdateItemIcon();
	OnSlotUpdated();
}

void UUIInventorySlotWidget::UpdateItemIcon()
{
	if (!img_item) return;
	// Cancel the previous async load and clear the old brush before reusing this slot.
	// SetBrushFromSoftTexture with a null path does not clear an existing brush.
	img_item->SetBrushFromTexture(nullptr);
	const bool bHasIcon = ContainsItem() && !Data.Icon.IsNull();
	img_item->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (bHasIcon) img_item->SetBrushFromSoftTexture(Data.Icon);
}

bool UUIInventorySlotWidget::RequestOperation(EUIInventoryOperation Operation, int32 Amount)
{
	UObject* Handler = OperationHandler.Get();
	if (!ContainsItem() || Amount < 0 || Amount > Data.Count
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
void UUIInventorySlotWidget::InitShortcut(UInputAction* Action) { InputAction = Action; OnShortcutUpdated(); }

void UUIInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateItemIcon();
	InputSubsystem = UPlayerInputSubsystem::GetInputSubsystem(this);
	if (auto* Input = InputSubsystem.Get())
	{
		Input->OnInputTypeChanged.AddUniqueDynamic(this, &ThisClass::OnInputTypeChanged);
		OnInputTypeChanged(Input->GetInputType());
	}
	if (btn_Root)
	{
		btn_Root->OnClicked.AddUniqueDynamic(this, &ThisClass::PrimaryAction);
		btn_Root->OnPressed.AddUniqueDynamic(this, &ThisClass::OnActionPressed);
		btn_Root->OnReleased.AddUniqueDynamic(this, &ThisClass::OnActionReleased);
		btn_Root->OnHovered.AddUniqueDynamic(this, &ThisClass::OnActionHover);
	}
}

void UUIInventorySlotWidget::NativeDestruct()
{
	ReleaseDragPress();
	if (auto* Input = InputSubsystem.Get()) Input->OnInputTypeChanged.RemoveDynamic(this, &ThisClass::OnInputTypeChanged);
	InputSubsystem.Reset();
	if (btn_Root)
	{
		btn_Root->OnClicked.RemoveDynamic(this, &ThisClass::PrimaryAction);
		btn_Root->OnPressed.RemoveDynamic(this, &ThisClass::OnActionPressed);
		btn_Root->OnReleased.RemoveDynamic(this, &ThisClass::OnActionReleased);
		btn_Root->OnHovered.RemoveDynamic(this, &ThisClass::OnActionHover);
	}
	Super::NativeDestruct();
}
