#include "Gameplay/Abilities/NGAimAbility.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Combat/NGCombat.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"


UNGAimingEffect::UNGAimingEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FInheritedTagContainer Tags;
	Tags.AddTag(TAG_NGWeaponAiming);
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("AimingTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}

UNGAimAbility::UNGAimAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGActivatingItem);
	ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive);
	ActivationBlockedTags.AddTag(TAG_NGReloadActive);
	ActivationBlockedTags.AddTag(TAG_NGRollActive);
	ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
}

void UNGAimAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	FNGWeaponAbilityContext Context;
	if (!ResolveWeaponContext(Handle, Info, Context) || !CommitAbility(Handle, Info, Activation))
	{
		EndAbility(Handle, Info, Activation, true, true);
		return;
	}
	if (!IsActive()) return;
	if (!ResolveWeaponContext(Handle, Info, Context))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	AimingASC = Info->AbilitySystemComponent;
	const auto ApplyingASC = AimingASC;
	const auto Applied = ApplyGameplayEffectToOwner(Handle, Info, Activation, GetDefault<UNGAimingEffect>(), GetAbilityLevel(Handle, Info));
	if (!IsActive())
	{
		if (auto* ASC = ApplyingASC.Get(); ASC && Applied.IsValid()) ASC->RemoveActiveGameplayEffect(Applied);
		return;
	}
	AimingEffect = Applied;
	if (!AimingEffect.IsValid() || !ResolveWeaponContext(Handle, Info, Context))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	BlockingTagHandle = AimingASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::BlockingTagChanged);
	auto* Task = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Task->OnRelease.AddDynamic(this, &ThisClass::OnAimInputReleased);
	Task->ReadyForActivation();
}

void UNGAimAbility::OnAimInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UNGAimAbility::BlockingTagChanged(FGameplayTag Tag, int32 Count)
{
	if (IsActive() && ((Count > 0 && Tag.MatchesAny(ActivationBlockedTags)) || (Tag == TAG_NGWeaponAiming && Count == 0)))
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UNGAimAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	if (!IsEndAbilityValid(Handle, Info)) return;
	if (auto* ASC = AimingASC.Get())
	{
		ASC->RegisterGenericGameplayTagEvent().Remove(BlockingTagHandle);
		const auto Effect = AimingEffect;
		AimingEffect.Invalidate();
		if (Effect.IsValid()) ASC->RemoveActiveGameplayEffect(Effect);
	}
	AimingASC.Reset();
	BlockingTagHandle.Reset();
	Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}
