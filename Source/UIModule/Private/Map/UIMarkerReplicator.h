#pragma once

#include "GameFramework/Info.h"
#include "UIMarkerReplicator.generated.h"

class UUIMarkerModel;

DECLARE_MULTICAST_DELEGATE_OneParam(FUIReplicatedMarkerEvent, UUIMarkerModel*);

UCLASS(NotPlaceable)
class AUIMarkerReplicator : public AInfo
{
	GENERATED_BODY()
public:
	AUIMarkerReplicator();
	bool PublishMarker(UUIMarkerModel* Model);
	void WithdrawMarker(UUIMarkerModel* Model);

	FUIReplicatedMarkerEvent OnMarkerReceived;
	FUIReplicatedMarkerEvent OnMarkerRemoved;

protected:
	virtual void OnSubobjectCreatedFromReplication(UObject* NewSubobject) override;
	virtual void OnSubobjectDestroyFromReplication(UObject* Subobject) override;
};
