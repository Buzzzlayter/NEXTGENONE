#pragma once

#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "CommonLoadingScreenSettings.generated.h"

class UUserWidget;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Loading Screen"))
class COMMONLOADINGSCREEN_API UCommonLoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="Display")
	TSoftClassPtr<UUserWidget> LoadingScreenWidget;

	UPROPERTY(Config, EditAnywhere, Category="Display")
	int32 LoadingScreenZOrder = 10000;

	UPROPERTY(Config, EditAnywhere, Category="Display", meta=(ClampMin="0", Units="s"))
	float HoldAfterLoad = 0.f;

protected:
	FName GetCategoryName() const override { return TEXT("Game"); }
};
