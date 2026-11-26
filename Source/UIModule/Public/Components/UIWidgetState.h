#pragma once

#include "GameplayTagContainer.h"
#include "HFSMState.h"
#include "LayoutTemplate/LayoutLayer.h"
#include "UIWidgetState.generated.h"

class UUIWidgetMounterComponent;
class UUserWidget;

UCLASS(Blueprintable, DisplayName="Widget State", PrioritizeCategories=("Widget"))
class UIMODULE_API UUIWidgetState : public UHFSMState
{
	GENERATED_BODY()

public:
	UUIWidgetState();

protected:
	void Enter() override;
	void Exit(FName Event) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widget", meta=(ExposeOnSpawn="true"))
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widget", meta=(ExposeOnSpawn="true"))
	FGameplayTag PlaceholderID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widget", meta=(ExposeOnSpawn="true"))
	ELayoutLayer Layer = ELayoutLayer::Content;

private:
	UPROPERTY(Transient)
	TObjectPtr<UObject> Context;

	UPROPERTY()
	TObjectPtr<UUIWidgetMounterComponent> WidgetMounter;
};
