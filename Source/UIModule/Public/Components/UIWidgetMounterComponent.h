#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HFSMStateComponent.h"
#include "LayoutTemplate/LayoutLayer.h"
#include "UIWidgetMounterComponent.generated.h"

class UUILayoutTemplate;
class UUserWidget;

UCLASS(Blueprintable, EditInlineNew, DisplayName = "Widget Mounter")
class UIMODULE_API UUIWidgetMounterComponent : public UHFSMStateComponent
{
	GENERATED_BODY()

public:
	virtual void Enter() override;
	virtual void Exit(FName Event) override;

	UFUNCTION(BlueprintPure)
	UUserWidget* GetWidget() const;

protected:
	UPROPERTY(EditAnywhere, Category = "Widget")
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, Category = "Widget")
	FGameplayTag PlaceholderID;

	UPROPERTY(EditAnywhere, Category = "Layout")
	bool bUseLayer = true;

	UPROPERTY(EditAnywhere, Category = "Layout", meta = (EditCondition = "bUseLayer"))
	ELayoutLayer Layer = ELayoutLayer::Content;

	UPROPERTY(EditAnywhere, Category = "Fallback")
	bool bAddToViewportIfNoLayout = false;

	UPROPERTY(EditAnywhere, Category = "Fallback", meta = (EditCondition = "bAddToViewportIfNoLayout"))
	int32 FallbackZOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Widget")
	UUserWidget* Widget = nullptr;

private:
	UPROPERTY()
	UUILayoutTemplate* MountedLayout = nullptr;

	UUILayoutTemplate* ResolveLayout() const;
	void MountWidget();
	void DemountWidget();
};
