#include "Gameplay/Teams/NGTeamComponent.h"

#include "Gameplay/GameModes/NextgenoneGameMode.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
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
	if (!Team || GetTeamId() == FGenericTeamId::NoTeam || Team->GetTeamId() == FGenericTeamId::NoTeam)
		return ETeamAttitude::Neutral;
	return GetTeamId() == Team->GetTeamId() ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

void UNGTeamComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority() || TeamId != FGenericTeamId::NoTeam.GetId()) return;
	const auto* State = Cast<APlayerState>(GetOwner());
	const auto* Controller = State ? Cast<AController>(State->GetOwner()) : Cast<AController>(GetOwner());
	const auto* Mode = GetWorld()->GetAuthGameMode<ANextgenoneGameMode>();
	if (Controller && Mode) SetTeamId(FGenericTeamId(Mode->GetDefaultTeamId(*Controller)));
}

void UNGTeamComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNGTeamComponent, TeamId);
}

void UNGTeamComponent::SetTeamId(FGenericTeamId NewTeamId)
{
	if (!GetOwner()->HasAuthority() || TeamId == NewTeamId.GetId()) return;
	TeamId = NewTeamId.GetId();
	GetOwner()->ForceNetUpdate();
}
