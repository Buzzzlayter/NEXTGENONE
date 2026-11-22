#pragma once

#include "Map/UIMarkerWidget.h"
#include "NGInteractionWidget.generated.h"

UCLASS()
class UNGInteractionWidget : public UUIMarkerWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
