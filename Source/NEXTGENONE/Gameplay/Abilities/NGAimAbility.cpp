#include "Gameplay/Abilities/NGAimAbility.h"
#include "Gameplay/Combat/NGCombat.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Abilities/NGALSActionAbilities.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_NGInputAim, "Input.Aim");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGWeaponAiming, "State.Weapon.Aiming");

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
	ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive);
	ActivationBlockedTags.AddTag(TAG_NGPunchActive);
	ActivationBlockedTags.AddTag(TAG_NGReloadActive);
	ActivationBlockedTags.AddTag(TAG_NGRollActive);
	ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
}

bool UNGAimAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	if (!Info || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
	const auto* Combat = Cast<UNGCombatComponent>(GetSourceObject(Handle, Info));
	const auto* Pawn = Cast<APawn>(Info->AvatarActor.Get());
	const auto* ASC = Info->AbilitySystemComponent.Get();
	return Combat && Pawn && Pawn->GetController() && Combat->GetOwner() == Pawn
		&& ASC && ASC->GetAvatarActor() == Pawn && Combat->GetEquipment().bEquipped;
}

void UNGAimAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	const auto* Combat = Info ? Cast<UNGCombatComponent>(GetSourceObject(Handle, Info)) : nullptr;
	if (!Info || !Combat || Combat->GetOwner() != Info->AvatarActor.Get() || !Combat->GetEquipment().bEquipped
		|| !CommitAbility(Handle, Info, Activation))
	{
		EndAbility(Handle, Info, Activation, true, true);
		return;
	}
	AimingASC = Info->AbilitySystemComponent;
	AimingEffect = ApplyGameplayEffectToOwner(Handle, Info, Activation, GetDefault<UNGAimingEffect>(), GetAbilityLevel(Handle, Info));
	if (!AimingEffect.IsValid())
	{
		EndAbility(Handle, Info, Activation, true, true);
		return;
	}
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
