#include "Gameplay/Abilities/NGEquipmentControlAbilities.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
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
	auto* Equipment = Avatar ? Avatar->FindComponentByClass<UNGEquipmentComponent>() : nullptr;
	if (!Info || !Info->IsLocallyControlled() || !Combat || !Equipment || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, false, true); return; }
	if (!IsActive()) return;
	ActiveCombat = Combat;
	ActiveEquipment = Equipment;
	const FNGEquipmentState State = Combat->GetEquipment();
	PrimaryRevision = State.Revision;
	if (!State.bEquipped || !HasCurrentWeapon())
	{ EndAbility(Handle, Info, Activation, false, true); return; }
	EquipmentChangedHandle = Equipment->OnEquipmentChanged.AddUObject(this, &ThisClass::EquipmentChanged);
	ContinuePrimaryInput();
	if (!IsActive()) return;
	auto* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Release->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	Release->ReadyForActivation();
}

bool UNGPrimaryAttackAbility::HasCurrentWeapon() const
{
	const auto* Combat = ActiveCombat.Get();
	const auto* Weapon = Combat ? Combat->GetWeaponInstance() : nullptr;
	return CurrentActorInfo && CurrentActorInfo->IsLocallyControlled() && Combat
		&& CurrentActorInfo->AvatarActor.Get() == Combat->GetOwner()
		&& Weapon && Weapon->IsCurrent() && ActiveEquipment.IsValid()
		&& PrimaryRevision == ActiveEquipment->GetHandsRevision();
}

void UNGPrimaryAttackAbility::EquipmentChanged()
{
	if (IsActive() && !HasCurrentWeapon())
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UNGPrimaryAttackAbility::ContinuePrimaryInput()
{
	if (!HasCurrentWeapon())
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true); return; }
	const FNGEquipmentState State = ActiveCombat->GetEquipment();
	ActiveCombat->RequestFire();
	if (!IsActive()) return;
	// A synchronous shot callback can change equipment before the next wait is created.
	if (!HasCurrentWeapon())
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true); return; }
	if (State.Definition->bAutomatic && FMath::IsFinite(State.Definition->FireInterval))
	{
		auto* Delay = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(.05f, State.Definition->FireInterval));
		Delay->OnFinish.AddDynamic(this, &ThisClass::ContinuePrimaryInput);
		Delay->ReadyForActivation();
	}
}

void UNGPrimaryAttackAbility::HandleInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UNGPrimaryAttackAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	if (auto* Equipment = ActiveEquipment.Get()) Equipment->OnEquipmentChanged.Remove(EquipmentChangedHandle);
	EquipmentChangedHandle.Reset();
	ActiveEquipment.Reset();
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
	auto* Equipment = Avatar ? Avatar->FindComponentByClass<UNGEquipmentComponent>() : nullptr;
	const bool bValid = Info && Info->IsLocallyControlled() && Equipment && CommitAbility(Handle, Info, Activation);
	if (bValid) Equipment->RequestEquip(bEquip);
	EndAbility(Handle, Info, Activation, false, !bValid);
}
