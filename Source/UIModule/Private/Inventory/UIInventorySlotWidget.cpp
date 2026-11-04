#include "Inventory/UIInventorySlotWidget.h"
#include "Inventory/UIInventoryOperationInterface.h"
#include "Components/Button.h"
#include "Input/PlayerInputSubsystem.h"

void UUIInventorySlotWidget::SetData(const FUIInventorySlotData& InData, UObject* InOperationHandler)
{
	Data = InData;
	OperationHandler = InOperationHandler;
	OnSlotUpdated();
}

bool UUIInventorySlotWidget::RequestOperation(EUIInventoryOperation Operation, int32 Amount)
{
	UObject* Handler = OperationHandler.Get();
	if (!ContainsItem() || Amount < 0 || Amount > Data.Count || !Data.AvailableOperations.Contains(Operation)
		|| !Handler || !Handler->GetClass()->ImplementsInterface(UUIInventoryOperationInterface::StaticClass())) return false;
	return IUIInventoryOperationInterface::Execute_RequestInventoryOperation(Handler, Data, Operation, Amount);
}

void UUIInventorySlotWidget::PrimaryAction() { RequestOperation(EUIInventoryOperation::PrimaryAction); }
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
