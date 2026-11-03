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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	FGameplayTag PlaceholderID;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	ELayoutLayer Layer = ELayoutLayer::Content;

	UPROPERTY(BlueprintReadOnly, Category = "Widget")
	UUserWidget* Widget = nullptr;

private:
	UPROPERTY()
	UUILayoutTemplate* MountedLayout = nullptr;

	UUILayoutTemplate* ResolveLayout() const;
	bool MountWidget();
	void DemountWidget(bool bImmediate = false);
};
