#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LayoutWidget.generated.h"

UCLASS(Blueprintable)
class UIMODULE_API ULayoutWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	bool bConsumeLoverLayers = false;
	
	UPROPERTY(EditAnywhere)
	float FadeOutDuration = 0.f;

	UFUNCTION(BlueprintImplementableEvent)
	void DoFadeIn();

	UFUNCTION(BlueprintImplementableEvent)
	void DoFadeOut();
};
