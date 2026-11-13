#include "Gameplay/AI/States/NGAIAbilityState.h"
#include "AbilitySystem/NGAbilitySystem.h"

void UNGAIAbilityState::StartAction()
{
	const uint64 StartedRun = RunSerial;
	if (HoldDuration >= ActionTimeout || !AbilityInput.Start(this, ActionASC.Get(), InputTag, HoldDuration,
		[this, StartedRun]() { return IsCurrentRun(StartedRun); },
		[this]() { return HasCurrentAvatar(); }, [this](bool bCancelled)
	{
		FinishAction(bCancelled ? ENGAIActionResult::Aborted : ENGAIActionResult::Succeeded,
			bCancelled ? FName(TEXT("Ability cancelled")) : FName(TEXT("Ability ended")));
	}))
		FinishAction(ENGAIActionResult::Failed, TEXT("Invalid input, hold duration or GAS activation rejected"));
}

void UNGAIAbilityState::ReleaseActionResources()
{
	AbilityInput.Reset();
}
