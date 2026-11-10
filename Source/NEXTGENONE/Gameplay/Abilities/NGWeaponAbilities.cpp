#include "Gameplay/Abilities/NGWeaponAbilities.h"
#include "Gameplay/Combat/NGCombat.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNGFireCooldownEffect::UNGFireCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f); // The ability sets the weapon's FireInterval on the spec.
	FInheritedTagContainer Tags; Tags.AddTag(TAG_NGFireCooldown);
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("FireCooldownTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}

UNGFireAbility::UNGFireAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	CooldownGameplayEffectClass = UNGFireCooldownEffect::StaticClass();
	ActivationRequiredTags.AddTag(TAG_NGWeaponAiming);
	ActivationBlockedTags.AddTag(TAG_NGRollActive); ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGActivatingItem);
	ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive);
	ActivationBlockedTags.AddTag(TAG_NGReloadActive);
}

UNGReloadAbility::UNGReloadAbility()
{
	ActivationBlockedTags.AddTag(TAG_NGRollActive); ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGActivatingItem);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive); ActivationBlockedTags.AddTag(TAG_NGDashActive); ActivationBlockedTags.AddTag(TAG_NGReloadActive);
	ActivationOwnedTags.AddTag(TAG_NGReloadActive);
}

void UNGReloadAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	FNGWeaponAbilityContext Context;
	FString DefinitionError;
	if (!Info || !Info->IsNetAuthority() || !ResolveWeaponContext(Handle, Info, Context)
		|| !Context.Definition->ValidateDefinition(DefinitionError)
		|| Context.Combat->GetMagazineAmmo() >= Context.Definition->MagazineCapacity
		|| Context.Combat->GetReserveAmmo() <= 0 || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	if (!IsActive()) return;
	// Commit and cosmetic callbacks can remove this spec or change the Avatar.
	if (!ResolveWeaponContext(Handle, Info, Context))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	const float Duration = Context.Definition->ReloadDuration;
	Context.Combat->MulticastReload(Context.Definition, Context.Combat->GetEquipment().Revision);
	if (!IsActive()) return;
	if (!ResolveWeaponContext(Handle, Info, Context))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	auto* Task = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	Task->OnFinish.AddDynamic(this, &ThisClass::FinishReload);
	Task->ReadyForActivation();
}

void UNGReloadAbility::FinishReload()
{
	FNGWeaponAbilityContext Context;
	const bool bReloaded = CurrentActorInfo && CurrentActorInfo->IsNetAuthority()
		&& ResolveWeaponContext(CurrentSpecHandle, CurrentActorInfo, Context)
		&& Context.Combat->CompleteReloadOnServer(Context.Source);
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bReloaded);
}

void UNGFireAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	FNGWeaponAbilityContext Context;
	FString DefinitionError;
	if (!Info || !Info->IsNetAuthority() || !ResolveWeaponContext(Handle, Info, Context)
		|| !Context.Definition->ValidateDefinition(DefinitionError) || Context.Combat->GetMagazineAmmo() <= 0
		|| !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	if (!IsActive()) return;
	// Commit callbacks may remove the spec or change equipment/Avatar.
	const bool bFired = ResolveWeaponContext(Handle, Info, Context) && Context.Combat->FireOnServer(Context.Source);
	if (IsActive()) EndAbility(Handle, Info, Activation, true, !bFired);
}

void UNGFireAbility::ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation) const
{
	FNGWeaponAbilityContext Context;
	if (!ResolveWeaponContext(Handle, Info, Context)) return;
	auto Spec = MakeOutgoingGameplayEffectSpec(Handle, Info, Activation, CooldownGameplayEffectClass, GetAbilityLevel(Handle, Info));
	if (!Spec.IsValid()) return;
	Spec.Data->SetDuration(Context.Definition->FireInterval, true);
	ApplyGameplayEffectSpecToOwner(Handle, Info, Activation, Spec);
}
