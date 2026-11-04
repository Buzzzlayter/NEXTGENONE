#include "Gameplay/Abilities/NGDashAbility.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGDashCooldown, "Cooldown.Movement.Dash");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGDashCue, "GameplayCue.Movement.Dash");

UNGDashCooldownEffect::UNGDashCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(.8f);
	FInheritedTagContainer Tags; Tags.AddTag(TAG_NGDashCooldown);
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DashCooldownTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}
UNGDashAbility::UNGDashAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	CooldownGameplayEffectClass = UNGDashCooldownEffect::StaticClass();
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive);
	ActivationOwnedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGPunchActive);
	FGameplayTagContainer Tags; Tags.AddTag(TAG_NGMovementAbility); SetAssetTags(Tags);
}
bool UNGDashAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	if (!Info || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
	const auto* Pawn = Cast<ANextgenoneCharacter>(Info->AvatarActor.Get());
	return Pawn && !Pawn->IsDead() && Pawn->GetAbilitySystemComponent() == Info->AbilitySystemComponent.Get()
		&& Pawn->GetMovementAction() == EALSMovementAction::None
		&& Pawn->GetMovementState() == EALSMovementState::InAir
		&& Pawn->GetStance() == EALSStance::Standing && Pawn->GetCharacterMovement()->IsFalling();
}
void UNGDashAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Pawn = Info ? Cast<ANextgenoneCharacter>(Info->AvatarActor.Get()) : nullptr;
	if (!Pawn || !Pawn->GetCharacterMovement()->IsFalling() || !FMath::IsFinite(Speed) || !FMath::IsFinite(Duration) || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	auto* Movement = Pawn->GetCharacterMovement();
	const FVector Direction = FVector::VectorPlaneProject(Pawn->GetActorForwardVector(), Movement->GetGravityDirection()).GetSafeNormal();
	if (Direction.IsNearlyZero()) { EndAbility(Handle, Info, Activation, true, true); return; }
	auto* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, TEXT("Dash"), Direction,
		FMath::Clamp(Speed, 1.f, 5000.f), FMath::Clamp(Duration, .05f, 1.f), false, nullptr,
		// Keep the flight momentum after the burst instead of clamping to walking speed.
		// Gravity stays enabled: the dash redirects horizontal flight without resetting jump/fall velocity.
		ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity, FVector::ZeroVector, 0.f, true);
	Task->OnFinish.AddDynamic(this, &ThisClass::FinishDash);
	Task->ReadyForActivation();
	if (IsActive())
	{
		FGameplayCueParameters Cue; Cue.Location = Pawn->GetActorLocation(); Cue.Normal = Direction;
		Cue.Instigator = Pawn;
		Info->AbilitySystemComponent->ExecuteGameplayCue(TAG_NGDashCue, Cue);
		OnDashStarted(Direction);
	}
}
void UNGDashAbility::FinishDash() { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); }
