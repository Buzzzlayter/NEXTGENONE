#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "WidgetPlaceholder.generated.h"

class UUserWidget;

UINTERFACE(Blueprintable, MinimalAPI)
class UWidgetPlaceholder : public UInterface
{
	GENERATED_BODY()
};

class IWidgetPlaceholder
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	FGameplayTag GetID() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool MountWidget(UUserWidget* Widget);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool DemountWidget(UUserWidget* Widget);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool ShowWidgets(UPARAM(ref) TArray<TSubclassOf<UUserWidget>>& Widgets);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool HideWidgets(UPARAM(ref) TArray<TSubclassOf<UUserWidget>>& Widgets);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void ShowAnimation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SendVisibility(bool bCondition, UPARAM(ref) const FGameplayTag& HidePesetTag);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Dispatch(FGameplayTag Event);
};
