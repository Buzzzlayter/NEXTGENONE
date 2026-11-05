
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/UIInventoryTypes.h"
#include "UIInventoryWidget.generated.h"

class UUIInventorySlotsList;

UCLASS(Abstract, Blueprintable)
class UIMODULE_API UUIInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Inventory") void SetInventoryData(FText Name, const TArray<FUIInventorySlotData>& InSlots, UObject* Handler);
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory") void OnInventoryUpdated();
protected:
	virtual void NativeConstruct() override;
	UPROPERTY(BlueprintReadOnly, Category="Inventory") FText InventoryName;
	UPROPERTY(BlueprintReadOnly, Category="Inventory") TArray<FUIInventorySlotData> SlotData;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UUIInventorySlotsList> SlotsList;
private:
	TWeakObjectPtr<UObject> OperationHandler;
};
