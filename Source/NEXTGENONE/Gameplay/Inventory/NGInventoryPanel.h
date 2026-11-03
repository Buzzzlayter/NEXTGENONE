#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "NGInventoryManagerComponent.h"
#include "NGInventoryPanel.generated.h"

class UVerticalBox;

UCLASS()
class UNGInventoryItemButton : public UButton
{
	GENERATED_BODY()
public:
	void InitializeItem(UNGInventoryItemInstance* InItem, APlayerController* InController,bool bInDrop=false);
private:
	TWeakObjectPtr<UNGInventoryItemInstance> Item;
	TWeakObjectPtr<APlayerController> Controller;
	bool bDrop=false;
	UFUNCTION() void Equip();
};

/** Inventory content. Its state component owns the window; the state owns input. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGInventoryPanel : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UVerticalBox> ItemsList;
private:
	TWeakObjectPtr<UNGInventoryManagerComponent> Inventory;
	TArray<TWeakObjectPtr<UNGInventoryItemInstance>> ObservedItems;
	UFUNCTION() void Refresh();
	UFUNCTION() void Changed(FNGInventoryChangeMessage Change);
	void UnbindItems();
};
