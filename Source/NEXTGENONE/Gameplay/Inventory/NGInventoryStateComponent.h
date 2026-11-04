#pragma once

#include "CoreMinimal.h"
#include "Components/UIWidgetMounterComponent.h"
#include "NGInventoryStateComponent.generated.h"

UENUM()
enum class ENGInventoryPanelSource : uint8 { Player, Interaction };

UCLASS(Blueprintable, EditInlineNew, DisplayName="Inventory Window")
class NEXTGENONE_API UNGInventoryStateComponent : public UUIWidgetMounterComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Inventory") ENGInventoryPanelSource Source = ENGInventoryPanelSource::Player;
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
};
