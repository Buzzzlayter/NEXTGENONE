#include "Inventory/UIInventoryWidget.h"
#include "Inventory/UIInventorySlotsList.h"
#include "Inventory/UIInventorySlotWidget.h"
#include "Inventory/UIInventoryDragDropOperation.h"
#include "Inventory/UIInventoryOperationInterface.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Framework/Application/SlateApplication.h"

void UUIInventoryWidget::SetInventoryData(FText Name, const TArray<FUIInventorySlotData>& InSlots, UObject* Handler)
{
	if (InSlots.IsEmpty()) { ReleasePress(); CancelOwnedDrag(); }
	InventoryName = Name;
	SlotData = InSlots;
	OperationHandler = Handler;
	UpdateSlots();
	OnInventoryUpdated();
}

void UUIInventoryWidget::UpdateSlots()
{
	if (!SlotsList) return;
	SlotsList->SetData(SlotData, OperationHandler.Get());
	for (int32 Index = 0; Index < SlotData.Num(); ++Index)
	{
		auto* SlotWidget = SlotsList->GetSlotWidget(Index);
		if (SlotWidget && !SlotWidget->DragDetected.IsBoundToObject(this))
			SlotWidget->DragDetected.BindUObject(this, &ThisClass::CreateDrag);
	}
	if (auto* Drag = GetOwnedDrag())
		if (SlotData.ContainsByPredicate([Drag](const FUIInventorySlotData& Entry)
		{
			return Entry.Inventory == Drag->GetSourceData().Inventory && Entry.SlotIndex == Drag->GetSourceData().SlotIndex
				&& Entry.Item == Drag->GetSourceData().Item;
		}))
			if (auto* Source = DragSourceSlot.Get()) Source->SetItemDragHidden(true);
}

FReply UUIInventoryWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FReply Reply = Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
	if (Reply.IsEventHandled() || !SlotsList) return Reply;
	auto* Source = SlotsList->FindSlotAt(Event.GetScreenSpacePosition());
	if (!Source || !Source->ContainsItem() || Source->IsItemDragHidden()) return Reply;
	if (Event.GetEffectingButton() == Source->GetSecondaryActionButton())
		return Source->RequestOperation(EUIInventoryOperation::SecondaryAction) ? FReply::Handled() : Reply;
	if (Event.GetEffectingButton() != Source->GetDragButton() || !OperationHandler.IsValid() || !GetOwningLocalPlayer()) return Reply;
	ReleasePress();
	PressedSlot = Source;
	PressedData = Source->GetData();
	Source->OnActionPressed();
	// Route drag detection through the source Slate widget to retain its starting geometry.
	return FReply::Handled().CaptureMouse(TakeWidget()).DetectDrag(Source->TakeWidget(), Source->GetDragButton());
}

void UUIInventoryWidget::ReleasePress()
{
	auto* Source = PressedSlot.Get();
	PressedSlot.Reset();
	PressedData = {};
	if (Source) Source->OnActionReleased();
}

FReply UUIInventoryWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	auto* Source = PressedSlot.Get();
	if (!Source || Event.GetEffectingButton() != Source->GetDragButton()) return Super::NativeOnMouseButtonUp(Geometry, Event);
	const auto Snapshot = PressedData;
	ReleasePress();
	const auto& Current = Source->GetData();
	if (SlotsList && SlotsList->FindSlotAt(Event.GetScreenSpacePosition()) == Source
		&& Current.Inventory == Snapshot.Inventory && Current.SlotIndex == Snapshot.SlotIndex && Current.Item == Snapshot.Item)
		Source->RequestOperation(EUIInventoryOperation::PrimaryAction);
	return FReply::Handled().ReleaseMouseCapture();
}

void UUIInventoryWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	ReleasePress();
	Super::NativeOnMouseCaptureLost(Event);
}

void UUIInventoryWidget::CreateDrag(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	auto* Source = PressedSlot.Get();
	const auto Snapshot = PressedData;
	ReleasePress();
	if (Operation || !Source || !Source->ContainsItem() || Source->GetData().Item != Snapshot.Item
		|| Source->GetData().Inventory != Snapshot.Inventory || Source->GetData().SlotIndex != Snapshot.SlotIndex) return;
	auto* Drag = NewObject<UUIInventoryDragDropOperation>(this);
	Drag->Initialize(Snapshot, GetOwningLocalPlayer());
	Drag->Pivot = EDragPivot::CenterCenter;
	Drag->Offset = FVector2D::ZeroVector;
	auto* Image = NewObject<UImage>(Drag);
	Image->SetBrushFromSoftTexture(Snapshot.Icon);
	const FVector2D VisualSize = Geometry.GetLocalSize() * Geometry.GetAccumulatedLayoutTransform().GetScale()
		/ FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), UE_SMALL_NUMBER);
	auto* Bounds = NewObject<USizeBox>(Drag);
	Bounds->SetVisibility(ESlateVisibility::HitTestInvisible);
	Bounds->SetWidthOverride(static_cast<float>(VisualSize.X));
	Bounds->SetHeightOverride(static_cast<float>(VisualSize.Y));
	Bounds->AddChild(Image);
	Drag->DefaultDragVisual = Bounds;
	Drag->OnDrop.AddDynamic(this, &ThisClass::DragFinished);
	Drag->OnDragCancelled.AddDynamic(this, &ThisClass::DragCancelled);
	DragSourceSlot = Source;
	Source->SetItemDragHidden(true);
	Operation = Drag;
}

bool UUIInventoryWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	auto* Drag = Cast<UUIInventoryDragDropOperation>(Operation);
	if (!Drag) return Super::NativeOnDrop(Geometry, Event, Operation);
	if (!Drag->IsFromPlayer(GetOwningLocalPlayer()) || !SlotsList) return false;
	auto* Target = SlotsList->FindSlotAt(Event.GetScreenSpacePosition());
	if (!Target) return false;
	const auto& Destination = Target->GetData();
	if (Destination.Inventory == Drag->GetSourceData().Inventory && Destination.SlotIndex == Drag->GetSourceData().SlotIndex)
	{
		Drag->DragCancelled(Event);
		return true;
	}
	auto* Handler = OperationHandler.Get();
	return Handler && Handler->Implements<UUIInventoryOperationInterface>()
		&& IUIInventoryOperationInterface::Execute_MoveInventoryItem(Handler, Drag->GetSourceData(), Destination, 0);
}

void UUIInventoryWidget::DragFinished(UDragDropOperation* Operation)
{
	Operation->OnDrop.RemoveDynamic(this, &ThisClass::DragFinished);
	Operation->OnDragCancelled.RemoveDynamic(this, &ThisClass::DragCancelled);
	DragSourceSlot.Reset(); // Accepted Drop keeps the icon hidden until the next changed snapshot.
}

void UUIInventoryWidget::DragCancelled(UDragDropOperation* Operation)
{
	if (auto* Source = DragSourceSlot.Get()) Source->SetItemDragHidden(false);
	DragFinished(Operation);
}

UUIInventoryDragDropOperation* UUIInventoryWidget::GetOwnedDrag() const
{
	if (!FSlateApplication::IsInitialized()) return nullptr;
	auto* Drag = Cast<UUIInventoryDragDropOperation>(UWidgetBlueprintLibrary::GetDragDroppingContent());
	return Drag && Drag->GetOuter() == this ? Drag : nullptr;
}

void UUIInventoryWidget::CancelOwnedDrag()
{
	if (GetOwnedDrag()) UWidgetBlueprintLibrary::CancelDragDrop();
}

void UUIInventoryWidget::NativeDestruct()
{
	ReleasePress();
	CancelOwnedDrag();
	Super::NativeDestruct();
}

void UUIInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateSlots();
}
