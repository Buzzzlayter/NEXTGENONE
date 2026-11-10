#include "Gameplay/Abilities/NGJetpackAbility.h"

#include "Gameplay/Character/NGCharacterMovementComponent.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Equipment/NGJetpackDefinition.h"
#include "GameplayTags/NGGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameFramework/Character.h"

UNGJetpackStateEffect::UNGJetpackStateEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FInheritedTagContainer Tags;
	Tags.AddTag(TAG_NGJetpackActive);
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("JetpackTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}

UNGJetpackAbility::UNGJetpackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_NGMovementAbility);
	SetAssetTags(Tags);
	for (const FGameplayTag Tag : {TAG_NGDead.GetTag(), TAG_NGRollActive.GetTag(), TAG_NGRagdollActive.GetTag(),
		TAG_NGDashActive.GetTag(), TAG_NGMantleActive.GetTag(), TAG_NGActivatingItem.GetTag()})
		ActivationBlockedTags.AddTag(Tag);
}

bool UNGJetpackAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	if (!Info || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
	const auto* Pawn = Cast<ACharacter>(Info->AvatarActor.Get());
	const auto* Movement = Pawn ? Cast<UNGCharacterMovementComponent>(Pawn->GetCharacterMovement()) : nullptr;
	const auto* Source = Cast<UNGEquipmentInstance>(GetSourceObject(Handle, Info));
    const auto* Definition = Source ? Cast<UNGJetpackDefinition>(Source->GetDefinition()) : nullptr;
    return Source && Source->IsCurrent() && Definition && Movement && Movement->CanStartJetpack() && Info->AbilitySystemComponent.IsValid()
		&& Info->AbilitySystemComponent->GetAvatarActor() == Pawn
		&& !Info->AbilitySystemComponent->HasMatchingGameplayTag(TAG_NGJetpackActive);
}

void UNGJetpackAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	const auto* Source = Info ? Cast<UNGEquipmentInstance>(GetSourceObject(Handle, Info)) : nullptr;
    const auto* Definition = Source ? Cast<UNGJetpackDefinition>(Source->GetDefinition()) : nullptr;
    auto* Pawn = Info ? Cast<ACharacter>(Info->AvatarActor.Get()) : nullptr;
	auto* Movement = Pawn ? Cast<UNGCharacterMovementComponent>(Pawn->GetCharacterMovement()) : nullptr;
	if (!Source || !Source->IsCurrent() || !Definition || !Movement || !Movement->CanStartJetpack() || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	if (!IsActive()) return;
	if (!Source->IsCurrent())
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	JetpackMovement = Movement;
	JetpackASC = Info->AbilitySystemComponent;
	const auto ApplyingASC = JetpackASC;
	const auto AppliedEffect = ApplyGameplayEffectToOwner(Handle, Info, Activation, GetDefault<UNGJetpackStateEffect>(), GetAbilityLevel(Handle, Info));
	if (!IsActive())
	{
		// Do not leak a handle returned after a reentrant cancellation during effect application.
		if (auto* ASC = ApplyingASC.Get(); ASC && AppliedEffect.IsValid()) ASC->RemoveActiveGameplayEffect(AppliedEffect);
		return;
	}
	StateEffect = AppliedEffect;
	if (!StateEffect.IsValid()) { EndAbility(Handle, Info, Activation, true, true); return; }
	if (!Source->IsCurrent() || !Movement->CanStartJetpack() || !JetpackASC.IsValid() || JetpackASC->GetAvatarActor() != Pawn
		|| JetpackASC->HasAnyMatchingGameplayTags(ActivationBlockedTags))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	Movement->SetJetpackRequested(true);
	BlockingTagHandle = JetpackASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::BlockingTagChanged);

	auto* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Release->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	Release->ReadyForActivation();
	if (!IsActive()) return;

	auto* Duration = UAbilityTask_WaitDelay::WaitDelay(this, Definition->MaxBurnDuration);
	Duration->OnFinish.AddDynamic(this, &ThisClass::FinishBurn);
	Duration->ReadyForActivation();
}

void UNGJetpackAbility::HandleInputReleased(float TimeHeld) { FinishBurn(); }
void UNGJetpackAbility::FinishBurn() { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); }

void UNGJetpackAbility::BlockingTagChanged(FGameplayTag Tag, int32 Count)
{
	if (IsActive() && ((Count > 0 && Tag.MatchesAny(ActivationBlockedTags)) || (Tag == TAG_NGJetpackActive && Count == 0)))
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UNGJetpackAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	if (!IsEndAbilityValid(Handle, Info)) return;
	if (auto* Movement = JetpackMovement.Get()) Movement->SetJetpackRequested(false);
	JetpackMovement.Reset();
	if (auto* ASC = JetpackASC.Get())
	{
		ASC->RegisterGenericGameplayTagEvent().Remove(BlockingTagHandle);
		const auto Effect = StateEffect;
		StateEffect.Invalidate();
		if (Effect.IsValid()) ASC->RemoveActiveGameplayEffect(Effect);
	}
	JetpackASC.Reset();
	BlockingTagHandle.Reset();
	Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}
