#pragma once

#include "CoreMinimal.h"
#include "Inventory/UIInventoryWidget.h"
#include "Inventory/UIInventoryOperationInterface.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "NGInventoryPanel.generated.h"

class UNGPlayerInventoryManagerComponent;

UCLASS(Abstract, Blueprintable)
class NEXTGENONE_API UNGInventoryPanel : public UUIInventoryWidget, public IUIInventoryOperationInterface
{
	GENERATED_BODY()
public:
	UNGInventoryPanel(const FObjectInitializer& Init);
	void SetInventorySource(UNGInventoryManagerComponent* Source);
	virtual bool RequestInventoryOperation_Implementation(const FUIInventorySlotData& SlotInfo, EUIInventoryOperation Operation, int32 Amount) override;
	virtual bool MoveInventoryItem_Implementation(const FUIInventorySlotData& Source, const FUIInventorySlotData& Destination, int32 Amount) override;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(EditDefaultsOnly, Category="Inventory") FText InventoryTitle = NSLOCTEXT("NEXTGENONE", "PlayerInventoryTitle", "Inventory");
	/** Empty displays every slot. Use Inventory.Slot.Action for the five-cell action panel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Inventory") FGameplayTag SlotFilter;
private:
	TWeakObjectPtr<UNGInventoryManagerComponent> Inventory;
	TArray<TWeakObjectPtr<UNGInventoryItemInstance>> ObservedItems;
	UFUNCTION() void Refresh();
	UFUNCTION() void Changed();
	UNGPlayerInventoryManagerComponent* ResolvePlayerInventory() const;
	void UnbindItems();
};
