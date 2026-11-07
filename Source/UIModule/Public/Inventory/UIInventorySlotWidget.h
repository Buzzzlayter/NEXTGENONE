
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/UIInventoryTypes.h"
#include "Input/InputData.h"
#include "InputCoreTypes.h"
#include "UIInventorySlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
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
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	UPROPERTY(EditDefaultsOnly, Category="Inventory|DragDrop") FKey DragButton = EKeys::LeftMouseButton;
	UPROPERTY(EditDefaultsOnly, Category="Inventory") FKey SecondaryActionButton = EKeys::RightMouseButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> btn_Root;
	/** Optional item image. Updated for every slot snapshot, including empty slots. */
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> img_item;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> txt_name;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> txt_count;
	UPROPERTY(BlueprintReadOnly, Category="Inventory") FUIInventorySlotData Data;
	UPROPERTY(BlueprintReadOnly, Category="Inventory") TObjectPtr<UInputAction> InputAction;
private:
	UPROPERTY(Transient) FUIInventorySlotData PointerDownData;
	bool bDragPending = false;
	void ReleaseDragPress();
	void UpdateItemIcon();
	void UpdateItemTextVisibility();
	TWeakObjectPtr<UObject> OperationHandler;
	TWeakObjectPtr<UPlayerInputSubsystem> InputSubsystem;
	UFUNCTION() void PrimaryAction();
};
