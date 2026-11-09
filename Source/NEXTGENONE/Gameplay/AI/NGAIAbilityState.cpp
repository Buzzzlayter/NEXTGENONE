#include "Gameplay/AI/NGAIAbilityState.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UNGAIAbilityState::StartAction()
{
	if (!InputTag.IsValid() || !FMath::IsFinite(HoldDuration) || HoldDuration < 0.f || HoldDuration >= ActionTimeout)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Invalid ability input or hold duration")); return; }
	int32 MatchingSpecs = 0;
	{
		FScopedAbilityListLock AbilityListLock(*ActionASC.Get());
		for (const auto& Spec : ActionASC->GetActivatableAbilities())
		{
			if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) continue;
			++MatchingSpecs;
			if (!Spec.IsActive() && !Spec.InputPressed && Spec.Ability
				&& Spec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
				AbilityHandle = Spec.Handle;
		}
	}
	if (MatchingSpecs != 1 || !AbilityHandle.IsValid())
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Expected one inactive InstancedPerActor input ability")); return; }
	const uint64 StartedRun = RunSerial;
	AbilityActivatedHandle = ActionASC->AbilityActivatedCallbacks.AddWeakLambda(this, [this, StartedRun](UGameplayAbility* Ability)
	{
		if (IsCurrentRun(StartedRun) && Ability && Ability->GetCurrentAbilitySpecHandle() == AbilityHandle)
			AbilityInstance = Ability;
	});
	AbilityEndedHandle = ActionASC->OnAbilityEnded.AddWeakLambda(this, [this, StartedRun](const FAbilityEndedData& Ended)
	{
		if (!IsCurrentRun(StartedRun) || Ended.AbilitySpecHandle != AbilityHandle) return;
		if (AbilityInstance.IsValid() && AbilityInstance.Get() != Ended.AbilityThatEnded.Get()) return;
		FinishAction(Ended.bWasCancelled ? ENGAIActionResult::Aborted : ENGAIActionResult::Succeeded,
			Ended.bWasCancelled ? FName(TEXT("Ability cancelled")) : FName(TEXT("Ability ended")));
	});
	bOwnsInput = true;
	ActionASC->AbilityInputTagPressed(InputTag);
	if (!IsCurrentRun(StartedRun)) return; // May have completed synchronously during press.
	const auto* Spec = ActionASC->FindAbilitySpecFromHandle(AbilityHandle);
	if (!Spec || !Spec->IsActive())
	{ FinishAction(ENGAIActionResult::Failed, TEXT("GAS activation rejected")); return; }
	AbilityInstance = Spec->GetPrimaryInstance();
	if (HoldDuration == 0.f) ReleaseInput();
	else GetWorld()->GetTimerManager().SetTimer(ReleaseTimer, FTimerDelegate::CreateWeakLambda(this, [this, StartedRun]()
	{
		if (IsCurrentRun(StartedRun)) ReleaseInput();
	}), HoldDuration, false);
}

void UNGAIAbilityState::ReleaseInput()
{
	if (!bOwnsInput) return;
	bOwnsInput = false;
	if (HasCurrentAvatar() && ActionASC.IsValid()) ActionASC->AbilityInputTagReleased(InputTag);
}

void UNGAIAbilityState::ReleaseActionResources()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReleaseTimer);
	if (auto* ASC = ActionASC.Get())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
		ASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
	}
	AbilityEndedHandle.Reset(); AbilityActivatedHandle.Reset();
	// Release can synchronously end the ability and trigger an unrelated new activation.
	bool bEndedDuringRelease = false;
	const auto CapturedASC = ActionASC;
	FDelegateHandle ReleaseEndedHandle;
	if (auto* ASC = CapturedASC.Get())
		ReleaseEndedHandle = ASC->OnAbilityEnded.AddLambda([this, &bEndedDuringRelease](const FAbilityEndedData& Ended)
		{
			if (Ended.AbilitySpecHandle == AbilityHandle) bEndedDuringRelease = true;
		});
	ReleaseInput();
	if (auto* ASC = CapturedASC.Get()) ASC->OnAbilityEnded.Remove(ReleaseEndedHandle);
	if (!bEndedDuringRelease && HasCurrentAvatar() && ActionASC.IsValid())
		if (const auto* Spec = ActionASC->FindAbilitySpecFromHandle(AbilityHandle); Spec && Spec->IsActive()
			&& Spec->GetPrimaryInstance() == AbilityInstance.Get())
			ActionASC->CancelAbilityHandle(AbilityHandle);
	AbilityHandle = FGameplayAbilitySpecHandle();
	AbilityInstance.Reset();
}
