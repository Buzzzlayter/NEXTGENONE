#include "Gameplay/UI/NGPingMarkerModel.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Teams/NGTeamComponent.h"

UNGPingMarkerModel::UNGPingMarkerModel()
{
	MapVisibility.bAlwaysVisible = true;
	WorldVisibility.bAlwaysVisible = true;
}

bool UNGPingMarkerModel::ShouldReplicateMarker() const
{
	return !GetRecipientGroup().IsNone();
}

bool UNGPingMarkerModel::IsVisibleTo(const APlayerController& Viewer) const
{
	const FName PingGroup = GetRecipientGroup();
	const auto* Team = UNGTeamComponent::Find(Viewer.GetPlayerState<APlayerState>());
	return Team && !PingGroup.IsNone() && Team->GetTeamName() == PingGroup;
}
