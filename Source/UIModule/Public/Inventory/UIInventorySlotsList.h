
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
	UFUNCTION(BlueprintPure, Category="Inventory") UUIInventorySlotWidget* GetSlotWidget(int32 Index) const;
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnListUpdated();
	UUIInventorySlotWidget* FindSlotAt(const FVector2D& ScreenPosition) const;
protected:
	virtual void NativeDestruct() override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UWrapBox> WrapBox;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory") TSubclassOf<UUIInventorySlotWidget> SlotClass;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Inventory") TArray<TObjectPtr<UUIInventorySlotWidget>> InventorySlots;
};
