#include "Gameplay/AI/States/NGAIAbilityInput.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool FNGAIAbilityInput::Start(UObject* InOwner, UNGAbilitySystemComponent* InASC, FGameplayTag Tag, float HoldDuration,
	TFunction<bool()> IsCurrentRun, TFunction<bool()> CanReleaseInput, TFunction<void(bool)> OnEnded)
{
	Reset();
	if (!IsValid(InOwner) || !InOwner->GetWorld() || !IsValid(InASC) || !Tag.IsValid()
		|| !FMath::IsFinite(HoldDuration) || HoldDuration < 0.f
		|| !IsCurrentRun || !CanReleaseInput || !IsCurrentRun()) return false;
	Owner = InOwner;
	ASC = InASC;
	InputTag = Tag;
	Completion = MoveTemp(OnEnded);
	CanRelease = MoveTemp(CanReleaseInput);
	int32 Matches = 0;
	{
		FScopedAbilityListLock Lock(*ASC.Get());
		for (const auto& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) continue;
			++Matches;
			if (!Spec.IsActive() && !Spec.InputPressed && Spec.Ability
				&& Spec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
				Handle = Spec.Handle;
		}
	}
	if (Matches != 1 || !Handle.IsValid()) return false;
	ActivatedHandle = ASC->AbilityActivatedCallbacks.AddWeakLambda(InOwner, [this, IsCurrentRun](UGameplayAbility* Ability)
	{
		if (Owner.IsValid() && IsCurrentRun() && Ability && Ability->GetCurrentAbilitySpecHandle() == Handle)
			Instance = Ability;
	});
	EndedHandle = ASC->OnAbilityEnded.AddWeakLambda(InOwner, [this, IsCurrentRun](const FAbilityEndedData& Ended)
	{
		if (!Owner.IsValid() || !IsCurrentRun() || Ended.AbilitySpecHandle != Handle
			|| (Instance.IsValid() && Instance.Get() != Ended.AbilityThatEnded.Get())) return;
		// Finishing the state resets this helper; invoke a local copy of the callback.
		auto Callback = Completion;
		if (Callback) Callback(Ended.bWasCancelled);
	});
	bOwnsInput = true;
	ASC->AbilityInputTagPressed(InputTag);
	if (!IsValid(InOwner) || !IsCurrentRun()) return true; // Can finish synchronously during press.
	const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	if (!Spec || !Spec->IsActive()) return false;
	Instance = Spec->GetPrimaryInstance();
	if (HoldDuration == 0.f) ReleaseInput();
	else InOwner->GetWorld()->GetTimerManager().SetTimer(ReleaseTimer, FTimerDelegate::CreateWeakLambda(InOwner, [this, IsCurrentRun]()
	{
		if (Owner.IsValid() && IsCurrentRun()) ReleaseInput();
	}), HoldDuration, false);
	return true;
}

void FNGAIAbilityInput::ReleaseInput()
{
	if (!bOwnsInput) return;
	bOwnsInput = false;
	if (Owner.IsValid() && CanRelease && CanRelease() && ASC.IsValid()) ASC->AbilityInputTagReleased(InputTag);
}

void FNGAIAbilityInput::Reset()
{
	if (Owner.IsValid() && Owner->GetWorld()) Owner->GetWorld()->GetTimerManager().ClearTimer(ReleaseTimer);
	const auto CapturedASC = ASC;
	if (auto* System = CapturedASC.Get())
	{
		System->OnAbilityEnded.Remove(EndedHandle);
		System->AbilityActivatedCallbacks.Remove(ActivatedHandle);
	}
	EndedHandle.Reset(); ActivatedHandle.Reset();
	bool bEndedDuringRelease = false;
	FDelegateHandle ReleaseEndedHandle;
	if (auto* System = CapturedASC.Get())
		ReleaseEndedHandle = System->OnAbilityEnded.AddLambda([this, &bEndedDuringRelease](const FAbilityEndedData& Ended)
		{
			if (Ended.AbilitySpecHandle == Handle) bEndedDuringRelease = true;
		});
	ReleaseInput();
	if (auto* System = CapturedASC.Get()) System->OnAbilityEnded.Remove(ReleaseEndedHandle);
	if (!bEndedDuringRelease && Owner.IsValid() && CanRelease && CanRelease() && CapturedASC.IsValid())
		if (const auto* Spec = CapturedASC->FindAbilitySpecFromHandle(Handle); Spec && Spec->IsActive()
			&& Spec->GetPrimaryInstance() == Instance.Get()) CapturedASC->CancelAbilityHandle(Handle);
	Handle = FGameplayAbilitySpecHandle();
	Instance.Reset(); ASC.Reset(); Owner.Reset();
	Completion = nullptr;
	CanRelease = nullptr;
	InputTag = FGameplayTag();
}
