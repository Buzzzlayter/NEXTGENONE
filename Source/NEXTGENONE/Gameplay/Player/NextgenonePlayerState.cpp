#include "Gameplay/Player/NextgenonePlayerState.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "AbilitySystem/NGAbilitySystemAvatar.h"
#include "Gameplay/Teams/NGTeamComponent.h"
#include "GameFramework/Pawn.h"

ANextgenonePlayerState::ANextgenonePlayerState()
{
	Team = CreateDefaultSubobject<UNGTeamComponent>(TEXT("Team"));
	AbilitySystem = CreateDefaultSubobject<UNGPlayerAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	HealthSet = CreateDefaultSubobject<UNGHealthSet>(TEXT("HealthSet"));
	Inventory = CreateDefaultSubobject<UNGPlayerInventoryManagerComponent>(TEXT("Inventory"));
	SetNetUpdateFrequency(30.f);
}

bool ANextgenonePlayerState::StartLife(APawn* Pawn)
{
	if (!HasAuthority() || bChangingLife || !IsValid(Pawn) || Pawn->GetPlayerState() != this
		|| (GetPawn() && GetPawn() != Pawn)) return false;
	auto* Avatar = Cast<INGAbilitySystemAvatar>(Pawn);
	if (!Avatar || !Avatar->CanInitializeGameplay()) return false;
	if (InitializedPawn == Pawn && AbilitySystem->GetAvatarActor() == Pawn) return true;

	AActor* Previous = AbilitySystem->GetAvatarActor();
	if (Previous && Previous != this && Previous != Pawn)
	{
		const auto* PreviousAvatar = Cast<INGAbilitySystemAvatar>(Previous);
		if (!PreviousAvatar) return false;
		if (!PreviousAvatar->CanReleaseGameplay())
		{
			PendingLifePawn = Pawn;
			return false;
		}
	}
	TGuardValue<bool> LifeGuard(bChangingLife, true);
	ReleaseAvatarResources(Previous);
	if (!IsValid(Pawn) || Pawn->GetPlayerState() != this || (GetPawn() && GetPawn() != Pawn)
		|| !Avatar->CanInitializeGameplay()) return false;

	const bool bNewLife = InitializedPawn != Pawn;
	InitializedPawn = Pawn;
	AbilitySystem->InitAbilityActorInfo(this, Pawn);
	if (bNewLife)
		Avatar->InitializeLifeAttributes(*AbilitySystem);
	PendingLifePawn.Reset();
	ForceNetUpdate();
	return true;
}

void ANextgenonePlayerState::ReleaseAvatarResources(AActor* Avatar)
{
	if (auto* Resources = Cast<INGAbilitySystemAvatar>(Avatar)) Resources->ReleaseGameplayResources();
	if (AbilitySystem->GetAvatarActor() != Avatar) return;
	AbilitySystem->ClearAbilityInput();
	AbilitySystem->CancelAllAbilities();
	if (AbilitySystem->GetAvatarActor() == Avatar) AbilitySystem->SetAvatarActor(nullptr);
}

bool ANextgenonePlayerState::ReleaseAvatar(APawn* ExpectedPawn)
{
	if (!HasAuthority() || bChangingLife || !ExpectedPawn || AbilitySystem->GetAvatarActor() != ExpectedPawn) return false;
	const auto* Avatar = Cast<INGAbilitySystemAvatar>(ExpectedPawn);
	if (!Avatar || !Avatar->CanReleaseGameplay()) return false;
	{
		TGuardValue<bool> LifeGuard(bChangingLife, true);
		ReleaseAvatarResources(ExpectedPawn);
	}
	ResumePendingLife(ExpectedPawn);
	return true;
}

void ANextgenonePlayerState::ResumePendingLife(APawn* PreviousPawn)
{
	if (!HasAuthority() || bChangingLife || !PreviousPawn) return;
	if (AbilitySystem->GetAvatarActor() && AbilitySystem->GetAvatarActor() != PreviousPawn) return;
	APawn* Pending = PendingLifePawn.Get();
	PendingLifePawn.Reset();
	if (IsValid(Pending) && GetPawn() == Pending && Pending->GetPlayerState() == this)
		if (auto* Avatar = Cast<INGAbilitySystemAvatar>(Pending)) Avatar->TryInitializeGameplay();
}

void ANextgenonePlayerState::EndPlay(EEndPlayReason::Type Reason)
{
	PendingLifePawn.Reset();
	if (HasAuthority())
	{
		TGuardValue<bool> LifeGuard(bChangingLife, true);
		ReleaseAvatarResources(AbilitySystem->GetAvatarActor());
	}
	Super::EndPlay(Reason);
}
