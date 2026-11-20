#pragma once

#include "CoreMinimal.h"
#include "Components/UIWidgetMounterComponent.h"
#include "NGInventoryStateComponent.generated.h"

UENUM()
enum class ENGInventoryPanelSource : uint8 { Player, Interaction };

UCLASS(Blueprintable, EditInlineNew, DisplayName="Inventory Window", PrioritizeCategories=("Inventory"))
class NEXTGENONE_API UNGInventoryStateComponent : public UUIWidgetMounterComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Inventory") ENGInventoryPanelSource Source = ENGInventoryPanelSource::Player;
	UPROPERTY(EditAnywhere, Category="Inventory", meta=(ClampMin="0", ToolTip="Slot size in Slate units. A zero axis keeps the size set in the slot Blueprint."))
	FVector2D SlotSize = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Inventory", meta=(DisplayName="Slot Columns", ClampMin="0", ToolTip="Number of columns in the slot grid. Zero uses the Wrap Box layout from the widget Blueprint."))
	int32 MaxSlotColumns = 0;
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
};
