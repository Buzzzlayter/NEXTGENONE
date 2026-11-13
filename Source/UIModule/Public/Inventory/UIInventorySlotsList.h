
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/UIInventoryTypes.h"
#include "UIInventorySlotsList.generated.h"

class UWrapBox;
class UUIInventorySlotWidget;

UCLASS(Abstract, Blueprintable)
class UIMODULE_API UUIInventorySlotsList : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Inventory") void SetData(const TArray<FUIInventorySlotData>& InSlots, UObject* InOperationHandler);
	UFUNCTION(BlueprintCallable, Category="Inventory") void SetFirstSlotFocused();
	UFUNCTION(BlueprintCallable, Category="Inventory") void SetWrapSize(float Size);
	UFUNCTION(BlueprintCallable, Category="Inventory") void SetInnerSlotPadding(FVector2D SlotPadding);
	UFUNCTION(BlueprintPure, Category="Inventory") UUIInventorySlotWidget* GetSlotWidget(int32 Index) const;
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnListUpdated();
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UWrapBox> WrapBox;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory") TSubclassOf<UUIInventorySlotWidget> SlotClass;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Inventory") TArray<TObjectPtr<UUIInventorySlotWidget>> InventorySlots;
private:
	UPROPERTY(Transient) TArray<FUIInventorySlotData> Slots;
	TWeakObjectPtr<UObject> OperationHandler;
	void Repaint();
	void CancelOwnedDragDrop() const;
};
