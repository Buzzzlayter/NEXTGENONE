#include "Gameplay/Abilities/NGMeleeAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Combat/NGDamage.h"
#include "Gameplay/Teams/NGTeamComponent.h"
#include "GameplayTags/NGGameplayTags.h"

UNGMeleeAbility::UNGMeleeAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	ActivationBlockedTags.AddTag(TAG_NGDead);
}

void UNGMeleeAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !ActorInfo->AnimInstance.IsValid()
		|| !AttackMontage || !FMath::IsFinite(HitTime) || HitTime <= 0.f || HitTime >= AttackMontage->GetPlayLength()
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsActive()) return;

	auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::FinishAttack);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::InterruptAttack);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::InterruptAttack);
	MontageTask->ReadyForActivation();
	if (!IsActive()) return;

	auto* HitTask = UAbilityTask_WaitDelay::WaitDelay(this, HitTime);
	HitTask->OnFinish.AddDynamic(this, &ThisClass::ApplyHit);
	HitTask->ReadyForActivation();
}

void UNGMeleeAbility::ApplyHit()
{
	auto* Attacker = Cast<APawn>(GetAvatarActorFromActorInfo());
	auto* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Attacker || !ASC || NGDamage::FindLivingAbilitySystem(Attacker) != ASC || !FMath::IsFinite(Reach) || Reach <= 0.f) return;

	const FVector Origin = Attacker->GetActorLocation();
	const FVector Forward = Attacker->GetActorForwardVector().GetSafeNormal2D();
	const auto* Team = UNGTeamComponent::Find(Attacker);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NGMelee), false, Attacker);
	TArray<AActor*> Attached;
	Attacker->GetAttachedActors(Attached);
	Query.AddIgnoredActors(Attached);
	TArray<FOverlapResult> Overlaps;
	auto* World = Attacker->GetWorld();
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Reach), Query);

	APawn* BestTarget = nullptr;
	double BestDistanceSquared = FMath::Square(static_cast<double>(Reach));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		auto* Target = Cast<APawn>(Overlap.GetActor());
		if (!NGDamage::FindLivingAbilitySystem(Target) || (Team && Team->GetAttitudeTowards(Target) == ETeamAttitude::Friendly)) continue;
		const FVector Offset = Target->GetActorLocation() - Origin;
		const double DistanceSquared = Offset.SizeSquared();
		if (DistanceSquared > BestDistanceSquared || FVector::DotProduct(Forward, Offset.GetSafeNormal2D()) < 0.5) continue;
		FHitResult Blocker;
		if (World->LineTraceSingleByChannel(Blocker, Origin, Target->GetActorLocation(), ECC_Visibility, Query) && Blocker.GetActor() != Target) continue;
		BestTarget = Target;
		BestDistanceSquared = DistanceSquared;
	}
	if (BestTarget) NGDamage::ApplyToPawn(BestTarget, ASC, Damage);
}

void UNGMeleeAbility::FinishAttack()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UNGMeleeAbility::InterruptAttack()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
