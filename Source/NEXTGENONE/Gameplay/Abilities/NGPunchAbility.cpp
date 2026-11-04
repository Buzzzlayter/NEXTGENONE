#include "Gameplay/Abilities/NGPunchAbility.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Engine/World.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGPunchCooldown, "Cooldown.Combat.Punch");

UNGPunchCooldownEffect::UNGPunchCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(.55f);
	FInheritedTagContainer Tags; Tags.AddTag(TAG_NGPunchCooldown);
	auto* Component = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("PunchCooldownTags"));
	GEComponents.Add(Component); Component->SetAndApplyTargetTagChanges(Tags);
}
UNGPunchAbility::UNGPunchAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	CooldownGameplayEffectClass = UNGPunchCooldownEffect::StaticClass();
	ActivationBlockedTags.AddTag(TAG_NGDead); ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive); ActivationBlockedTags.AddTag(TAG_NGPunchActive);
	ActivationOwnedTags.AddTag(TAG_NGPunchActive);
}
bool UNGPunchAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	if (!Info || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
	const auto* Pawn = Cast<ANextgenoneCharacter>(Info->AvatarActor.Get());
	return Pawn && !Pawn->IsDead() && Pawn->GetController() && Pawn->GetAbilitySystemComponent() == Info->AbilitySystemComponent.Get()
		&& !Pawn->Combat->GetEquipment().bEquipped && Pawn->GetMovementAction() == EALSMovementAction::None
		&& Pawn->GetMovementState() == EALSMovementState::Grounded;
}
bool UNGPunchAbility::IsStrikePawnValid(const FGameplayAbilityActorInfo* Info) const
{
	const auto* Pawn = StrikePawn.Get();
	return Pawn && Info && Info->IsNetAuthority() && Info->AvatarActor.Get() == Pawn && !Pawn->IsDead()
		&& Pawn->GetController() && Pawn->GetAbilitySystemComponent() == Info->AbilitySystemComponent.Get()
		&& !Pawn->Combat->GetEquipment().bEquipped && Pawn->GetMovementAction() == EALSMovementAction::None
		&& Pawn->GetMovementState() == EALSMovementState::Grounded;
}
void UNGPunchAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	StrikePawn = Info ? Cast<ANextgenoneCharacter>(Info->AvatarActor.Get()) : nullptr;
	if (!IsStrikePawnValid(Info) || !FMath::IsFinite(Damage) || !FMath::IsFinite(Reach) || !FMath::IsFinite(HitRadius)
		|| !FMath::IsFinite(Windup) || !FMath::IsFinite(KnockbackSpeed) || !FMath::IsFinite(KnockbackLift)
		|| !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	auto* Pawn = StrikePawn.Get();
	Pawn->Combat->MulticastPunchStarted(Pawn->GetActorForwardVector(), PunchMontage);
	auto* Task = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Clamp(Windup, 0.f, .4f));
	Task->OnFinish.AddDynamic(this, &ThisClass::ResolveStrike); Task->ReadyForActivation();
}
void UNGPunchAbility::ResolveStrike()
{
	if (!IsStrikePawnValid(CurrentActorInfo))
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
	auto* Pawn = StrikePawn.Get();
	const FVector Up = -Pawn->GetCharacterMovement()->GetGravityDirection();
	const FVector Forward = FVector::VectorPlaneProject(Pawn->GetActorForwardVector(), Up).GetSafeNormal();
	const FVector Start = Pawn->GetActorLocation() + Up * 35.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGPunch), false, Pawn);
	TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult Hit;
	// The first Visibility blocker wins: one victim per strike, walls block the sphere.
	GetWorld()->SweepSingleByChannel(Hit, Start, Start + Forward * FMath::Clamp(Reach, 50.f, 200.f), FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(FMath::Clamp(HitRadius, 10.f, 50.f)), Params);
	auto* Target = Cast<ANextgenoneCharacter>(Hit.GetActor());
	if (Target && Target != Pawn && !Target->IsDead() && Target->GetController())
	{
		const FVector ToTarget = Target->GetActorLocation() - Pawn->GetActorLocation();
		FHitResult Obstruction;
		GetWorld()->LineTraceSingleByChannel(Obstruction, Start, Hit.ImpactPoint, ECC_Visibility, Params);
		const bool bVisible = !Obstruction.bBlockingHit || Obstruction.GetActor() == Target;
		auto* TargetASC = Target->GetAbilitySystemComponent();
		if (bVisible && FVector::DotProduct(Forward, ToTarget.GetSafeNormal()) > .25f && TargetASC
			&& UNGCombatComponent::ApplyDamageToPawn(Target, CurrentActorInfo->AbilitySystemComponent.Get(), FMath::Clamp(Damage, 1.f, 1000.f), &Hit))
		{
			Pawn->Combat->MulticastPunchHit(Target, Hit.ImpactPoint);
			// Damage/death callbacks may invalidate a life. Never push a replacement avatar or revive a corpse.
			if (IsValid(Target) && !Target->IsDead() && TargetASC->GetAvatarActor() == Target && Target->GetController())
			{
				// End movement root-motion tasks so they cannot override this authoritative knockback.
				FGameplayTagContainer MovementTags; MovementTags.AddTag(TAG_NGMovementAbility);
				TargetASC->CancelAbilities(&MovementTags);
				const FVector TargetUp = -Target->GetCharacterMovement()->GetGravityDirection();
				FVector Direction = FVector::VectorPlaneProject(ToTarget, TargetUp).GetSafeNormal();
				if (Direction.IsNearlyZero()) Direction = Forward;
				Target->LaunchCharacter(Direction * FMath::Clamp(KnockbackSpeed, 0.f, 2000.f)
					+ TargetUp * FMath::Clamp(KnockbackLift, 0.f, 600.f), true, true);
				Target->ForceNetUpdate();
			}
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
void UNGPunchAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	StrikePawn.Reset(); Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}
