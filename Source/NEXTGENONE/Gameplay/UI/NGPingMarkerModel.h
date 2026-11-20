#pragma once

#include "Map/UIMarkerModel.h"
#include "NGPingMarkerModel.generated.h"

UCLASS()
class NEXTGENONE_API UNGPingMarkerModel : public UUIMarkerModel
{
	GENERATED_BODY()

public:
	UNGPingMarkerModel();

	virtual bool ShouldReplicateMarker() const override;
	virtual bool IsVisibleTo(const APlayerController& Viewer) const override;
};
