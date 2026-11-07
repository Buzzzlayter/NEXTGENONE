#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Abilities/NGAimAbility.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Abilities/NGALSActionAbilities.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Inventory/NGInventoryItemInstance.h"

UNGFireAbility::UNGFireAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	ActivationRequiredTags.AddTag(TAG_NGWeaponAiming);
	ActivationBlockedTags.AddTag(TAG_NGRollActive); ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive);
	ActivationBlockedTags.AddTag(TAG_NGPunchActive);
	ActivationBlockedTags.AddTag(TAG_NGReloadActive);
}

UNGReloadAbility::UNGReloadAbility()
{
	ActivationBlockedTags.AddTag(TAG_NGRollActive); ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	ActivationBlockedTags.AddTag(TAG_NGDead); ActivationBlockedTags.AddTag(TAG_NGPunchActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive); ActivationBlockedTags.AddTag(TAG_NGDashActive); ActivationBlockedTags.AddTag(TAG_NGReloadActive);
	ActivationOwnedTags.AddTag(TAG_NGReloadActive);
}

void UNGReloadAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Combat = Cast<UNGCombatComponent>(GetSourceObject(Handle, Info));
	const auto* Def = Combat ? Combat->GetEquipment().Definition.Get() : nullptr;
	FString DefinitionError;
	if (!Info || !Info->IsNetAuthority() || !Combat || Combat->GetOwner() != Info->AvatarActor.Get() || !Combat->GetEquipment().bEquipped
		|| !Def || !Def->ValidateDefinition(DefinitionError) || Combat->GetMagazineAmmo() >= Def->MagazineCapacity || Combat->GetReserveAmmo() <= 0 || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	ReloadCombat = Combat; ReloadItem = Combat->GetEquippedItem();
	Combat->MulticastReload();
	auto* Task = UAbilityTask_WaitDelay::WaitDelay(this, Def->ReloadDuration);
	Task->OnFinish.AddDynamic(this, &ThisClass::FinishReload); Task->ReadyForActivation();
}

void UNGReloadAbility::FinishReload()
{
	bool bReloaded = false;
	if (auto* Combat = ReloadCombat.Get())
		if (CurrentActorInfo && Combat->GetOwner() == CurrentActorInfo->AvatarActor.Get()) bReloaded = Combat->CompleteReloadOnServer(ReloadItem.Get());
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bReloaded);
}

void UNGReloadAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	ReloadCombat.Reset(); ReloadItem.Reset(); Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}

void UNGFireAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Combat = Cast<UNGCombatComponent>(GetSourceObject(Handle, Info));
	if (Info && Info->IsNetAuthority() && Combat && Combat->GetOwner() == Info->AvatarActor.Get()) Combat->FireOnServer();
	EndAbility(Handle, Info, Activation, true, false);
}
