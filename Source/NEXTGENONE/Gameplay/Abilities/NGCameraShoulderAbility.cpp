#include "Gameplay/Abilities/NGCameraShoulderAbility.h"

#include "Character/ALSBaseCharacter.h"
#include "GameplayTags/NGGameplayTags.h"

UNGCameraShoulderAbility::UNGCameraShoulderAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationBlockedTags.AddTag(TAG_NGDead);
}

void UNGCameraShoulderAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Character = Info && Info->IsLocallyControlledPlayer() ? Cast<AALSBaseCharacter>(Info->AvatarActor.Get()) : nullptr;
	if (!IsValid(Character) || !CommitAbility(Handle, Info, Activation))
	{
		EndAbility(Handle, Info, Activation, false, true);
		return;
	}

	Character->CameraTapAction();
	EndAbility(Handle, Info, Activation, false, false);
}
