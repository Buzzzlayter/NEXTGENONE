#include "Gameplay/Abilities/NGEquipmentControlAbilities.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Combat/NGCombat.h"
#include "GameplayTags/NGGameplayTags.h"

UNGPrimaryAttackAbility::UNGPrimaryAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	FGameplayTagContainer AbilityType;
	AbilityType.AddTag(TAG_NGPrimaryInputAbility);
	SetAssetTags(AbilityType);
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGActivatingItem);
}

void UNGPrimaryAttackAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	AActor* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
	UNGCombatComponent* Combat = Avatar ? Avatar->FindComponentByClass<UNGCombatComponent>() : nullptr;
	if (!Info || !Info->IsLocallyControlled() || !Combat || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, false, true); return; }
	ActiveCombat = Combat;
	Combat->StartPrimaryInput();
	auto* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Release->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	Release->ReadyForActivation();
}

void UNGPrimaryAttackAbility::HandleInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UNGPrimaryAttackAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	if (UNGCombatComponent* Combat = ActiveCombat.Get()) Combat->StopPrimaryInput();
	ActiveCombat.Reset();
	Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}

UNGEquipmentControlAbility::UNGEquipmentControlAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationBlockedTags.AddTag(TAG_NGDead);
	CancelAbilitiesWithTag.AddTag(TAG_NGPrimaryInputAbility);
}

void UNGEquipmentControlAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	AActor* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
	UNGCombatComponent* Combat = Avatar ? Avatar->FindComponentByClass<UNGCombatComponent>() : nullptr;
	const bool bValid = Info && Info->IsLocallyControlled() && Combat && CommitAbility(Handle, Info, Activation);
	if (bValid) Combat->RequestEquip(bEquip);
	EndAbility(Handle, Info, Activation, false, !bValid);
}
