#pragma once

#include "CoreMinimal.h"
#include "Components/UIWidgetMounterComponent.h"
#include "NGInventoryStateComponent.generated.h"

UCLASS(Blueprintable, EditInlineNew, DisplayName="Inventory Window")
class NEXTGENONE_API UNGInventoryStateComponent : public UUIWidgetMounterComponent
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
};
