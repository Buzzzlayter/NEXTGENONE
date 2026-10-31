#include "NextgenonePlayerState.h"
#include "NextgenoneCharacter.h"

ANextgenonePlayerState::ANextgenonePlayerState()
{
	AbilitySystem = CreateDefaultSubobject<UNGAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true); AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	HealthSet = CreateDefaultSubobject<UNGHealthSet>(TEXT("HealthSet"));
	SetNetUpdateFrequency(30.f);
}
void ANextgenonePlayerState::StartLife(ANextgenoneCharacter* Pawn)
{
	if (!HasAuthority() || !Pawn || InitializedPawn == Pawn) return;
	InitializedPawn = Pawn; ++LifeSerial;
	AbilitySystem->InitAbilityActorInfo(this, Pawn);
	AbilitySystem->SetLooseGameplayTagCount(TAG_NGDead, 0, EGameplayTagReplicationState::TagOnly);
	if (!bPlayerAbilitiesGranted)
	{
		if (PlayerAbilitySet) PlayerAbilitySet->Give(AbilitySystem, this, PlayerHandles);
		bPlayerAbilitiesGranted = true;
	}
	AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UNGInitHealthEffect>(), 1.f, AbilitySystem->MakeEffectContext());
	ForceNetUpdate();
}
