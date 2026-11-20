#include "Gameplay/Abilities/NGAimAbility.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/Combat/NGWeaponActor.h"
#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"


UNGAimingEffect::UNGAimingEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	GameplayCues.Emplace(TAG_NGLaserSightCue, 0.f, 1.f);
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
	ActivationBlockedTags.AddTag(TAG_NGRollActive);
	ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
}

bool UNGAimAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
	return !Info->AbilitySystemComponent->HasMatchingGameplayTag(TAG_NGReloadActive) || CanAimWhileReloading(Handle, Info);
}

bool UNGAimAbility::CanAimWhileReloading(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info) const
{
	FNGWeaponAbilityContext Context;
	return ResolveWeaponContext(Handle, Info, Context) && Context.Definition->bAllowAimWhileReloading;
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
	auto Spec = MakeOutgoingGameplayEffectSpec(Handle, Info, Activation, UNGAimingEffect::StaticClass(), GetAbilityLevel(Handle, Info));
	if (!Spec.IsValid())
	{
		EndAbility(Handle, Info, Activation, true, true);
		return;
	}
	auto EffectContext = Spec.Data->GetContext();
	EffectContext.AddSourceObject(Context.Weapon);
	const auto Applied = ApplyGameplayEffectSpecToOwner(Handle, Info, Activation, Spec);
	if (!IsActive())
	{
		if (auto* ASC = ApplyingASC.Get(); ASC && Applied.IsValid()) ASC->RemoveActiveGameplayEffect(Applied);
		return;
	}
	AimingEffect = Applied;
	if (!AimingEffect.IsValid() || !ResolveWeaponContext(Handle, Info, Context))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	BlockingTagHandle = AimingASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::BlockingTagChanged);
	if (Info->AvatarActor->GetNetMode() != NM_DedicatedServer)
	{
		AimingWeapon = Context.Weapon;
		AimSource = Info->AvatarActor->FindComponentByClass<UNGAimComponent>();
		if (auto* Aim = AimSource.Get())
		{
			Aim->OnAimUpdated.AddUObject(this, &ThisClass::AimUpdated);
			Aim->SetComponentTickEnabled(true);
		}
	}
	auto* Task = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Task->OnRelease.AddDynamic(this, &ThisClass::OnAimInputReleased);
	Task->ReadyForActivation();
}

void UNGAimAbility::OnAimInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UNGAimAbility::AimUpdated()
{
	if (!IsActive()) return;
	if (auto* Weapon = AimingWeapon.Get())
	{
		const auto* Aim = AimSource.Get();
		FVector Origin, Direction;
		if (Aim && Aim->GetViewRay(Origin, Direction)) Weapon->UpdateAimPresentation(Origin, Direction);
		else Weapon->SetLaserSightVisible(false);
	}
}

void UNGAimAbility::BlockingTagChanged(FGameplayTag Tag, int32 Count)
{
	if (!IsActive()) return;
	const bool bReloadBlocksAim = Count > 0 && Tag == TAG_NGReloadActive
		&& !CanAimWhileReloading(CurrentSpecHandle, CurrentActorInfo);
	if (bReloadBlocksAim || (Count > 0 && Tag.MatchesAny(ActivationBlockedTags)) || (Tag == TAG_NGWeaponAiming && Count == 0))
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UNGAimAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	if (!IsEndAbilityValid(Handle, Info)) return;
	if (auto* Aim = AimSource.Get())
	{
		Aim->OnAimUpdated.RemoveAll(this);
		Aim->SetComponentTickEnabled(false);
	}
	AimSource.Reset();
	if (auto* Weapon = AimingWeapon.Get()) Weapon->SetLaserSightVisible(false);
	AimingWeapon.Reset();
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
