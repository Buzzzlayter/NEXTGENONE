#include "Map/UIMarkerReplicator.h"

#include "Engine/World.h"
#include "Map/UIMarkerModel.h"
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "Net/Subsystems/NetworkSubsystem.h"

AUIMarkerReplicator::AUIMarkerReplicator()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateUsingRegisteredSubObjectList = true;
}

bool AUIMarkerReplicator::PublishMarker(UUIMarkerModel* Model)
{
	const FName RecipientGroup = Model->GetRecipientGroup();
	if (RecipientGroup.IsNone())
	{
		AddReplicatedSubObject(Model);
	}
	else
	{
		auto* Network = UWorld::GetSubsystem<UNetworkSubsystem>(GetWorld());
		if (!Network) return false;
		Network->GetNetConditionGroupManager().RegisterSubObjectInGroup(Model, RecipientGroup);
		AddReplicatedSubObject(Model, COND_NetGroup);
		UE::Net::FReplicationSystemUtil::UpdateSubObjectGroupMemberships(Model, GetWorld());
	}
	ForceNetUpdate();
	return true;
}

void AUIMarkerReplicator::WithdrawMarker(UUIMarkerModel* Model)
{
	DestroyReplicatedSubObjectOnRemotePeers(Model);
	if (auto* Network = UWorld::GetSubsystem<UNetworkSubsystem>(GetWorld()))
		Network->GetNetConditionGroupManager().UnregisterSubObjectFromAllGroups(Model);
	ForceNetUpdate();
}

void AUIMarkerReplicator::OnSubobjectCreatedFromReplication(UObject* NewSubobject)
{
	Super::OnSubobjectCreatedFromReplication(NewSubobject);
	if (auto* Model = Cast<UUIMarkerModel>(NewSubobject))
		OnMarkerReceived.Broadcast(Model);
}

void AUIMarkerReplicator::OnSubobjectDestroyFromReplication(UObject* Subobject)
{
	if (auto* Model = Cast<UUIMarkerModel>(Subobject))
		OnMarkerRemoved.Broadcast(Model);
	Super::OnSubobjectDestroyFromReplication(Subobject);
}
