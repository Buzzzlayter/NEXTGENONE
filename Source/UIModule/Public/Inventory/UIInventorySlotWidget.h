
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/UIInventoryTypes.h"
#include "InputCoreTypes.h"
#include "UIInventorySlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DELEGATE_ThreeParams(FInventoryDragDetected, const FGeometry&, const FPointerEvent&, UDragDropOperation*&);

UCLASS(Abstract, Blueprintable)
class UIMODULE_API UUIInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	FInventoryDragDetected DragDetected;
	const FUIInventorySlotData& GetData() const { return Data; }
	FKey GetDragButton() const { return DragButton; }
	FKey GetSecondaryActionButton() const { return SecondaryActionButton; }
	bool IsItemDragHidden() const { return bItemDragHidden; }
	void SetItemDragHidden(bool bHidden);
	UFUNCTION(BlueprintCallable, Category="Inventory") void SetData(const FUIInventorySlotData& InData, UObject* InOperationHandler);
	UFUNCTION(BlueprintPure, Category="Inventory") bool ContainsItem() const { return IsValid(Data.Item) && Data.Count > 0; }
	UFUNCTION(BlueprintCallable, Category="Inventory") bool RequestOperation(EUIInventoryOperation Operation, int32 Amount=0);
	UFUNCTION(BlueprintCallable, Category="Inventory") bool MoveItemHere(const FUIInventorySlotData& Source, int32 Amount=0);
	UFUNCTION(BlueprintCallable, Category="Inventory") void FocusSlot();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnSlotUpdated();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnActionPressed();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnActionReleased();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnActionHover();
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	UPROPERTY(EditDefaultsOnly, Category="Inventory|DragDrop") FKey DragButton = EKeys::LeftMouseButton;
	UPROPERTY(EditDefaultsOnly, Category="Inventory") FKey SecondaryActionButton = EKeys::RightMouseButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> btn_Root;
	/** Optional item image. Updated for every slot snapshot, including empty slots. */
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> img_item;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> txt_name;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> txt_count;
	UPROPERTY(BlueprintReadOnly, Category="Inventory") FUIInventorySlotData Data;
private:
	void UpdateItemIcon();
	void UpdateItemText();
	void UpdateItemVisibility();
	bool bItemDragHidden = false;
	TWeakObjectPtr<UObject> OperationHandler;
	UFUNCTION() void PrimaryAction();
};
