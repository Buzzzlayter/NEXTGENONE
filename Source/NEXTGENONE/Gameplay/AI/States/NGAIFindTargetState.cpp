#include "Gameplay/AI/States/NGAIFindTargetState.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UNGAIFindTargetState::StartAction()
{
	const uint64 StartedRun = RunSerial;
	GetWorld()->GetTimerManager().SetTimer(SearchTimer, FTimerDelegate::CreateWeakLambda(this, [this, StartedRun]()
	{
		if (IsCurrentRun(StartedRun)) CheckTarget();
	}), .2f, true);
	CheckTarget();
}

void UNGAIFindTargetState::CheckTarget()
{
	const auto* Target = ActionController->GetMemory()->GetTarget();
	if (Target && (!bRequireVisible || Target->bVisible))
		FinishAction(ENGAIActionResult::Succeeded, TEXT("Target acquired"));
}

void UNGAIFindTargetState::ReleaseActionResources()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SearchTimer);
}
