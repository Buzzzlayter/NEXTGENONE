#include "Gameplay/Teams/NGTeamComponent.h"

#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/Core/Misc/NetConditionGroupManager.h"
#include "Net/UnrealNetwork.h"

UNGTeamComponent::UNGTeamComponent()
{
	SetIsReplicatedByDefault(true);
}

UNGTeamComponent* UNGTeamComponent::Find(const AActor* Actor)
{
	if (!IsValid(Actor)) return nullptr;
	if (auto* Team = Actor->FindComponentByClass<UNGTeamComponent>()) return Team;
	const APlayerState* State = nullptr;
	if (const auto* Pawn = Cast<APawn>(Actor)) State = Pawn->GetPlayerState();
	else if (const auto* Controller = Cast<AController>(Actor)) State = Controller->GetPlayerState<APlayerState>();
	return State ? State->FindComponentByClass<UNGTeamComponent>() : nullptr;
}

ETeamAttitude::Type UNGTeamComponent::GetAttitudeTowards(const AActor* Other) const
{
	const auto* Team = Find(Other);
	if (!Team || TeamName.IsNone() || Team->TeamName.IsNone())
		return ETeamAttitude::Neutral;
	return TeamName == Team->TeamName ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

void UNGTeamComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority() || TeamName.IsNone()) return;
	if (UE::Net::IsSpecialNetConditionGroup(TeamName))
	{
		SetTeamName(NAME_None);
		return;
	}
	if (auto* Player = GetOwningPlayerController()) Player->IncludeInNetConditionGroup(TeamName);
}

void UNGTeamComponent::EndPlay(EEndPlayReason::Type Reason)
{
	if (GetOwner()->HasAuthority() && !TeamName.IsNone())
		if (auto* Player = GetOwningPlayerController()) Player->RemoveFromNetConditionGroup(TeamName);
	Super::EndPlay(Reason);
}

APlayerController* UNGTeamComponent::GetOwningPlayerController() const
{
	if (const auto* State = Cast<APlayerState>(GetOwner())) return Cast<APlayerController>(State->GetOwner());
	return Cast<APlayerController>(GetOwner());
}

void UNGTeamComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNGTeamComponent, TeamName);
}

void UNGTeamComponent::SetTeamName(FName NewTeamName)
{
	if (!GetOwner()->HasAuthority() || TeamName == NewTeamName || UE::Net::IsSpecialNetConditionGroup(NewTeamName)) return;

	const FName PreviousTeamName = TeamName;
	TeamName = NewTeamName;
	if (auto* Player = GetOwningPlayerController())
	{
		if (!PreviousTeamName.IsNone()) Player->RemoveFromNetConditionGroup(PreviousTeamName);
		if (!TeamName.IsNone()) Player->IncludeInNetConditionGroup(TeamName);
	}
	GetOwner()->ForceNetUpdate();
}
