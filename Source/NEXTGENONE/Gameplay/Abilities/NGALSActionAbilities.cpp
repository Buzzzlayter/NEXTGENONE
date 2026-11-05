#include "Gameplay/Abilities/NGALSActionAbilities.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_NGInputRoll, "Input.Roll");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGInputRagdoll, "Input.Ragdoll");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGRollActive, "State.Roll");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGRagdollActive, "State.Ragdoll");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ALSActionCooldown, "Cooldown.Movement.ALSAction");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGRagdollAbility, "Ability.Type.Ragdoll");

namespace
{
	bool CanStartALSAction(const FGameplayAbilityActorInfo* Info)
	{
		const auto* Pawn = Info ? Cast<ANextgenoneCharacter>(Info->AvatarActor.Get()) : nullptr;
		const auto* ASC = Info ? Info->AbilitySystemComponent.Get() : nullptr;
		return Pawn && ASC && ASC->GetAvatarActor() == Pawn && Pawn->GetAbilitySystemComponent() == ASC
			&& Pawn->GetController() && !Pawn->IsDead() && Pawn->GetHealth() > 0.f
			&& Pawn->GetMovementAction() == EALSMovementAction::None
			&& (Pawn->GetMovementState() == EALSMovementState::Grounded || Pawn->GetMovementState() == EALSMovementState::InAir);
	}
}

UNGALSActionCooldownEffect::UNGALSActionCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(.3f);
	FInheritedTagContainer Tags; Tags.AddTag(TAG_ALSActionCooldown);
	auto* Component = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTags"));
	GEComponents.Add(Component); Component->SetAndApplyTargetTagChanges(Tags);
}

UNGRagdollStateEffect::UNGRagdollStateEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FInheritedTagContainer Tags; Tags.AddTag(TAG_NGRagdollActive);
	auto* Component = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("RagdollTags"));
	GEComponents.Add(Component); Component->SetAndApplyTargetTagChanges(Tags);
}

UNGRollAbility::UNGRollAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// Execute the task on the owning client too: ASC montage replication skips its owner.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	CooldownGameplayEffectClass = UNGALSActionCooldownEffect::StaticClass();
	for (const FGameplayTag Tag : {TAG_NGDead.GetTag(), TAG_NGDashActive.GetTag(), TAG_NGMantleActive.GetTag(), TAG_NGPunchActive.GetTag(), TAG_NGReloadActive.GetTag()}) ActivationBlockedTags.AddTag(Tag);
	ActivationBlockedTags.AddTag(TAG_NGRagdollActive); ActivationBlockedTags.AddTag(TAG_NGRollActive);
	ActivationOwnedTags.AddTag(TAG_NGRollActive);
	FGameplayTagContainer Tags; Tags.AddTag(TAG_NGMovementAbility); SetAssetTags(Tags);
}

bool UNGRollAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	return CanStartALSAction(Info) && FMath::IsFinite(PlayRate) && PlayRate >= .1f && PlayRate <= 3.f
		&& CastChecked<ANextgenoneCharacter>(Info->AvatarActor.Get())->GetMovementState() == EALSMovementState::Grounded
		&& Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags);
}

void UNGRollAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Pawn = Info ? Cast<ANextgenoneCharacter>(Info->AvatarActor.Get()) : nullptr;
	auto* Montage = Pawn ? Pawn->GetRollAnimation() : nullptr;
	const auto* Mesh = Pawn && Pawn->GetMesh() ? Pawn->GetMesh()->GetSkeletalMeshAsset() : nullptr;
	if (!Info || !IsValid(Montage) || !Mesh
		|| Montage->GetSkeleton() != Mesh->GetSkeleton() || !FMath::IsFinite(PlayRate) || PlayRate < .1f || PlayRate > 3.f
		|| (Info->IsNetAuthority() && (!CanStartALSAction(Info) || !CommitAbility(Handle, Info, Activation))))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	auto* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, PlayRate);
	Task->OnCompleted.AddDynamic(this, &ThisClass::Completed);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::Interrupted);
	Task->OnCancelled.AddDynamic(this, &ThisClass::Interrupted);
	Task->ReadyForActivation();
}

void UNGRollAbility::Completed()
{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); }
void UNGRollAbility::Interrupted()
{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }

UNGRagdollAbility::UNGRagdollAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// Server starts the state; the owner's instance only forwards subsequent input events.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	CooldownGameplayEffectClass = UNGALSActionCooldownEffect::StaticClass();
	for (const FGameplayTag Tag : {TAG_NGDead.GetTag(), TAG_NGDashActive.GetTag(), TAG_NGMantleActive.GetTag(), TAG_NGPunchActive.GetTag(), TAG_NGReloadActive.GetTag()}) ActivationBlockedTags.AddTag(Tag);
	ActivationBlockedTags.AddTag(TAG_NGRollActive); ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
	// UI's local cancellation of movement input must not end this server-owned lifetime.
	FGameplayTagContainer Tags; Tags.AddTag(TAG_NGRagdollAbility); SetAssetTags(Tags);
}

bool UNGRagdollAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	return CanStartALSAction(Info) && Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags);
}

void UNGRagdollAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	if (!Info || (Info->IsNetAuthority() && (!CanStartALSAction(Info) || !CommitAbility(Handle, Info, Activation))))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	RagdollASC = Info->AbilitySystemComponent;
	if (Info->IsNetAuthority())
	{
		RagdollEffect = ApplyGameplayEffectToOwner(Handle, Info, Activation, GetDefault<UNGRagdollStateEffect>(), GetAbilityLevel(Handle, Info));
		if (!RagdollEffect.IsValid()) { EndAbility(Handle, Info, Activation, true, true); return; }
	}
	auto* Task = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	Task->OnPress.AddDynamic(this, &ThisClass::GetUpPressed);
	Task->ReadyForActivation();
}

void UNGRagdollAbility::GetUpPressed(float TimeWaited)
{
	if (!IsActive() || !CurrentActorInfo) return;
	if (const auto* ASC = RagdollASC.Get(); !CurrentActorInfo->IsNetAuthority() || (ASC && ASC->HasMatchingGameplayTag(TAG_ALSActionCooldown)))
	{
		auto* Task = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
		Task->OnPress.AddDynamic(this, &ThisClass::GetUpPressed);
		Task->ReadyForActivation();
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UNGRagdollAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	if (!IsEndAbilityValid(Handle, Info)) return;
	const auto Effect = RagdollEffect; RagdollEffect.Invalidate();
	auto* ASC = RagdollASC.Get(); RagdollASC.Reset();
	if (ASC && Effect.IsValid()) ASC->RemoveActiveGameplayEffect(Effect);
	Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}
