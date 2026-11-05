#include "Gameplay/Abilities/NGJumpAbility.h"
#include "Gameplay/Abilities/NGALSActionAbilities.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Controller.h"

namespace
{
	bool CanHandleJumpInput(const ANextgenoneCharacter* Pawn, const FGameplayAbilityActorInfo* Info)
	{
		if (!Pawn || !Info || Pawn->IsDead() || Pawn->GetAbilitySystemComponent() != Info->AbilitySystemComponent.Get()
			|| !Pawn->GetController() || Pawn->GetController()->IsMoveInputIgnored()
			|| Pawn->GetMovementAction() != EALSMovementAction::None) return false;
		if (Pawn->GetMovementState() == EALSMovementState::Ragdoll) return true;
		return Pawn->GetMovementState() == EALSMovementState::Grounded
			&& (Pawn->GetStance() == EALSStance::Crouching || (Pawn->GetStance() == EALSStance::Standing && Pawn->CanJump()));
	}
}

UNGJumpAbility::UNGJumpAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGRollActive);
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
	return CanHandleJumpInput(Pawn, Info);
}
void UNGJumpAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Pawn = Info ? Cast<ANextgenoneCharacter>(Info->AvatarActor.Get()) : nullptr;
	if (!CanHandleJumpInput(Pawn, Info) || !CommitAbility(Handle, Info, Activation))
	{
		EndAbility(Handle, Info, Activation, true, true);
		return;
	}
	if (Pawn->GetMovementState() == EALSMovementState::Ragdoll)
	{
		// GAS validates on the server; ALS distributes get-up once, without a second client RPC.
		if (Info->IsNetAuthority()) Pawn->ReplicatedRagdollEnd();
		EndAbility(Handle, Info, Activation, true, false);
		return;
	}
	if (Pawn->GetStance() == EALSStance::Crouching)
	{
		Pawn->UnCrouch();
		EndAbility(Handle, Info, Activation, true, false);
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
	if (!IsEndAbilityValid(Handle, Info)) return;
	// Never resolve a new avatar here: this ability may be ending during respawn.
	const bool bWasJumping = JumpPawn.IsValid();
	if (auto* Pawn = JumpPawn.Get()) Pawn->StopJumping();
	JumpPawn.Reset();
	Super::EndAbility(Handle, Info, Activation, bReplicateEndAbility, bWasCancelled);
	if (bWasJumping) OnJumpStopped(bWasCancelled);
}
