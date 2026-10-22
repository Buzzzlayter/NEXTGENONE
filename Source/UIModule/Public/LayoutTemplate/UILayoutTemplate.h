#pragma once

#include "CoreMinimal.h"
#include "LayoutLayer.h"
#include "LayoutWidget.h"
#include "GameplayTagContainer.h"
#include "Components/HFSMPlayerStateComponent.h"
#include "UILayoutTemplate.generated.h"

USTRUCT()
struct FUIModuleWidgetDemountData
{
	GENERATED_BODY()

	UPROPERTY()
	FString StateName;

	UPROPERTY()
	FGameplayTag PlaceholderID;

	UPROPERTY()
	TWeakObjectPtr<UWidget> Placeholder;

	UPROPERTY()
	TWeakObjectPtr<UUserWidget> Widget;
};

UCLASS(BlueprintType)
class UIMODULE_API UUILayoutTemplate : public UHFSMPlayerStateComponent
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	bool MountWidgetTo(UUserWidget* Widget, FGameplayTag PlaceholderID);

	UFUNCTION(BlueprintCallable)
	void DemountWidget(UUserWidget* Widget, FGameplayTag PlaceholderID);

	UFUNCTION(BlueprintCallable)
	void SetOpacity(float Opacity);
	
	virtual bool Enter() override;
	virtual void AbortEnter() override;
	virtual void Tick(float deltaTime) override;
	virtual void Exit(FName event) override;
	void LayersUpdate(bool& bOutConsumeDrawing);
	void SetVisible(bool isVisible) const;
	bool IsLayer(const ELayoutLayer& InLayer) const;

	bool operator<(const UUILayoutTemplate& Other) const;

	const TMap<FGameplayTag, UWidget*>& GetPlaceholders();

protected:
	UPROPERTY(EditDefaultsOnly)
	ELayoutLayer Layer = ELayoutLayer::Content;

	UPROPERTY(EditAnywhere)
	TSubclassOf<ULayoutWidget> Template;

	UPROPERTY()
	ULayoutWidget* TemplateWidget;

	UPROPERTY()
	TMap<FGameplayTag, UWidget*> Placeholders;

	UPROPERTY()
	TArray<FUIModuleWidgetDemountData> WidgetsAwaitingDemount;

	static void DemountWidgetsImmediate(const TArray<FUIModuleWidgetDemountData>& WidgetsAwaitingDemount);
};
