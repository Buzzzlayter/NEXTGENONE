// Adapted from R5InventorySlotWidget; R5BL/consumable/backend dependencies are intentionally excluded.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/UIInventoryTypes.h"
#include "Input/InputData.h"
#include "UIInventorySlotWidget.generated.h"

class UButton;
class UInputAction;
class UPlayerInputSubsystem;

UCLASS(Abstract, Blueprintable)
class UIMODULE_API UUIInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Inventory") void SetData(const FUIInventorySlotData& InData, UObject* InOperationHandler);
	UFUNCTION(BlueprintPure, Category="Inventory") bool ContainsItem() const { return IsValid(Data.Item) && Data.Count > 0; }
	UFUNCTION(BlueprintCallable, Category="Inventory") bool RequestOperation(EUIInventoryOperation Operation, int32 Amount=0);
	UFUNCTION(BlueprintCallable, Category="Inventory") bool MoveItemHere(const FUIInventorySlotData& Source, int32 Amount=0);
	UFUNCTION(BlueprintCallable, Category="Inventory") void FocusSlot();
	UFUNCTION(BlueprintCallable, Category="Inventory") void InitShortcut(UInputAction* Action);
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnSlotUpdated();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnActionPressed();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnActionReleased();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnActionHover();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnShortcutUpdated();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnInputTypeChanged(EInputType InputType);
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> btn_Root;
	UPROPERTY(BlueprintReadOnly, Category="Inventory") FUIInventorySlotData Data;
	UPROPERTY(BlueprintReadOnly, Category="Inventory") TObjectPtr<UInputAction> InputAction;
private:
	TWeakObjectPtr<UObject> OperationHandler;
	TWeakObjectPtr<UPlayerInputSubsystem> InputSubsystem;
	UFUNCTION() void PrimaryAction();
};
