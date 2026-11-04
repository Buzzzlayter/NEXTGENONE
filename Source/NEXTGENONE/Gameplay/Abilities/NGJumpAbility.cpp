#include "Gameplay/Abilities/NGJumpAbility.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"

UNGJumpAbility::UNGJumpAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive);
	ActivationBlockedTags.AddTag(TAG_NGPunchActive);
	FGameplayTagContainer Tags; Tags.AddTag(TAG_NGMovementAbility); SetAssetTags(Tags);
}
bool UNGJumpAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	if (!Info || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
	const auto* Pawn = Cast<ANextgenoneCharacter>(Info->AvatarActor.Get());
	return Pawn && !Pawn->IsDead() && Pawn->GetAbilitySystemComponent() == Info->AbilitySystemComponent.Get()
		&& Pawn->GetMovementAction() == EALSMovementAction::None
		&& Pawn->GetMovementState() == EALSMovementState::Grounded
		&& Pawn->GetStance() == EALSStance::Standing && Pawn->CanJump();
}
void UNGJumpAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Pawn = Info ? Cast<ANextgenoneCharacter>(Info->AvatarActor.Get()) : nullptr;
	if (!Pawn || !CommitAbility(Handle, Info, Activation))
	{
		EndAbility(Handle, Info, Activation, true, true);
		return;
	}
	JumpPawn = Pawn;
	Pawn->Jump();
	auto* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
	ReleaseTask->ReadyForActivation();
	if (IsActive()) OnJumpStarted();
}
void UNGJumpAbility::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
void UNGJumpAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayAbilityActivationInfo Activation, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Never resolve a new avatar here: this ability may be ending during respawn.
	if (auto* Pawn = JumpPawn.Get()) Pawn->StopJumping();
	JumpPawn.Reset();
	Super::EndAbility(Handle, Info, Activation, bReplicateEndAbility, bWasCancelled);
	OnJumpStopped(bWasCancelled);
}
