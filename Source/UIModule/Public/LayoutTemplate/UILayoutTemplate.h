#pragma once

#include "CoreMinimal.h"
#include "LayoutLayer.h"
#include "LayoutWidget.h"
#include "GameplayTagContainer.h"
#include "HFSMStateComponent.h"
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
class UIMODULE_API UUILayoutTemplate : public UHFSMStateComponent
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void MountWidgetTo(UUserWidget* Widget, FGameplayTag PlaceholderID);

	UFUNCTION(BlueprintCallable)
	void DemountWidget(UUserWidget* Widget, FGameplayTag PlaceholderID);

	UFUNCTION(BlueprintCallable)
	void SetOpacity(float Opacity);
	
	virtual void Enter() override;
	virtual void Tick(float deltaTime) override;
	virtual void Exit(FName event) override;
	void LayersUpdate(int32 Index, bool& bOutConsumeDrawing);
	void Hide() const;
	void Show() const;
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

	int32 ZOrder = 0;

	UPROPERTY()
	TMap<FGameplayTag, UWidget*> Placeholders;

	UPROPERTY()
	TArray<FUIModuleWidgetDemountData> WidgetsAwaitingDemount;

	static void DemountWidgetsImmediate(const TArray<FUIModuleWidgetDemountData>& WidgetsAwaitingDemount);
};
