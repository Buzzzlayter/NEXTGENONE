#include "NextgenonePlayerState.h"
#include "NextgenoneCharacter.h"
#include "NGJumpAbility.h"
#include "NGDashAbility.h"
#include "NGMantleAbility.h"
#include "NGWeaponPickup.h"
#include "NGInteraction.h"
#include "NGPunchAbility.h"

ANextgenonePlayerState::ANextgenonePlayerState()
{
	JumpAbility = UNGJumpAbility::StaticClass();
	DefaultPlayerAbilitySet = TSoftObjectPtr<UNGAbilitySet>(FSoftObjectPath(TEXT("/Game/Gameplay/Abilities/DA_PlayerAbilities.DA_PlayerAbilities")));
	AbilitySystem = CreateDefaultSubobject<UNGAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true); AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	HealthSet = CreateDefaultSubobject<UNGHealthSet>(TEXT("HealthSet"));
	Inventory = CreateDefaultSubobject<UNGInventoryManagerComponent>(TEXT("Inventory"));
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
		const UNGAbilitySet* Set = PlayerAbilitySet ? PlayerAbilitySet.Get() : DefaultPlayerAbilitySet.LoadSynchronous();
		if (Set) Set->Give(AbilitySystem, this, PlayerHandles);
		for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(TAG_NGInputPickup))
			{ Spec.GetDynamicSpecSourceTags().AddTag(TAG_NGInputInteract); AbilitySystem->MarkAbilitySpecDirty(Spec); }
		auto EnsureDefaultGrant = [this](FGameplayTag Tag, TSubclassOf<UGameplayAbility> Ability)
		{
			if (!Ability) return;
			for (const FGameplayAbilitySpec& Existing : AbilitySystem->GetActivatableAbilities())
				if (Existing.GetDynamicSpecSourceTags().HasTagExact(Tag)) return;
			FGameplayAbilitySpec Spec(Ability, 1);
			Spec.SourceObject = this;
			Spec.GetDynamicSpecSourceTags().AddTag(Tag);
			PlayerHandles.Abilities.Add(AbilitySystem->GiveAbility(Spec));
		};
		EnsureDefaultGrant(TAG_NGInputJump, JumpAbility);
		EnsureDefaultGrant(TAG_NGInputDash, UNGDashAbility::StaticClass());
		EnsureDefaultGrant(TAG_NGInputMantle, UNGMantleAbility::StaticClass());
		EnsureDefaultGrant(TAG_NGInputInteract, UNGInteractionAbility::StaticClass());
		EnsureDefaultGrant(TAG_NGInputPunch, UNGPunchAbility::StaticClass());
		bPlayerAbilitiesGranted = true;
	}
	AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UNGInitHealthEffect>(), 1.f, AbilitySystem->MakeEffectContext());
	ForceNetUpdate();
}
