#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/UIInventoryTypes.h"
#include "UIInventoryWidget.generated.h"

class UUIInventorySlotsList;
class UUIInventorySlotWidget;
class UUIInventoryDragDropOperation;

UCLASS(Abstract, Blueprintable)
class UIMODULE_API UUIInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetSlotLayout(const FVector2D& Size, int32 Columns);
	UFUNCTION(BlueprintCallable, Category="Inventory") void SetInventoryData(FText Name, const TArray<FUIInventorySlotData>& InSlots, UObject* Handler);
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnInventoryUpdated();
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;
	UPROPERTY(BlueprintReadOnly, Category="Inventory") FText InventoryName;
	UPROPERTY(BlueprintReadOnly, Category="Inventory") TArray<FUIInventorySlotData> SlotData;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UUIInventorySlotsList> SlotsList;
private:
	TWeakObjectPtr<UObject> OperationHandler;
	TWeakObjectPtr<UUIInventorySlotWidget> PressedSlot;
	TWeakObjectPtr<UUIInventorySlotWidget> DragSourceSlot;
	UPROPERTY(Transient) FUIInventorySlotData PressedData;
	void UpdateSlots();
	void ReleasePress();
	void CreateDrag(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation);
	void CancelOwnedDrag();
	UUIInventoryDragDropOperation* GetOwnedDrag() const;
	UFUNCTION() void DragFinished(UDragDropOperation* Operation);
	UFUNCTION() void DragCancelled(UDragDropOperation* Operation);
};
