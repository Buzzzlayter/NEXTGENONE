#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "LayoutLayer.h"
#include "HFSMStateComponent.h"
#include "LayoutTemplatesSorter.generated.h"

class UUILayoutTemplate;

UCLASS()
class UIMODULE_API ULayoutTemplatesSorter : public UHFSMStateComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetLayoutsVisibility(bool IsVisible);

	UFUNCTION(BlueprintCallable)
	void SetCustomVisibility(bool bIsEnable, const FGameplayTag& HidePresetTag);

	UFUNCTION(BlueprintPure)
	UUILayoutTemplate* GetLayer(ELayoutLayer Layer);

	void AddLayout(UUILayoutTemplate* LayoutTemplate);
	void RemoveLayout(UUILayoutTemplate* LayoutTemplate);

private:
	bool bLayoutsVisible = true;
	void Update() const;

	UPROPERTY()
	TArray<UUILayoutTemplate*> TemplatesList;
};
