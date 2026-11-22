#pragma once

#include "Blueprint/UserWidget.h"
#include "CommonLoadingScreenWidget.generated.h"

UCLASS(Blueprintable)
class COMMONLOADINGSCREEN_API UCommonLoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	TSharedRef<SWidget> RebuildWidget() override;
};
