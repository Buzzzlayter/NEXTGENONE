#include "Gameplay/AI/States/NGAITacticalDecisionState.h"

#include "Gameplay/AI/NGAIEvents.h"
#include "Gameplay/AI/States/NGAITacticalPlanningState.h"
#include "GameFramework/Pawn.h"
#include "GCFSMUtilities.h"

namespace
{
	const FName PlanningFSM(TEXT("Planning"));
}

TArray<FName> UNGAITacticalDecisionState::GetDeclaredTransitionEvents() const
{
	return {NGAIEvents::Search, NGAIEvents::HoldAngle, NGAIEvents::GroundFlank, NGAIEvents::Reload, NGAIEvents::Failed};
}

TArray<FName> UNGAITacticalMobilityDecisionState::GetDeclaredTransitionEvents() const
{
	return {NGAIEvents::Search, NGAIEvents::GroundFlank, NGAIEvents::EquipJetpack, NGAIEvents::Fly, NGAIEvents::Wait, NGAIEvents::Failed};
}

TArray<FName> UNGAITacticalWeaponDecisionState::GetDeclaredTransitionEvents() const
{
	return {NGAIEvents::HoldAngle, NGAIEvents::Reload, NGAIEvents::EquipWeapon, NGAIEvents::Wait, NGAIEvents::Failed};
}

void UNGAITacticalDecisionState::StartAction()
{
	const auto* AttackFSM = GetParentFSM();
	auto* Behavior = AttackFSM ? AttackFSM->GetContainerState() : nullptr;
	EGCFSMGetActiveStateResult LookupResult;
	auto* Planner = Behavior ? Cast<UNGAITacticalPlanningState>(UGCFSMUtilities::GetActiveState(
		Behavior, PlanningFSM, UNGAITacticalPlanningState::StaticClass(), LookupResult)) : nullptr;
	if (!Planner)
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("Planning FSM unavailable"));
		return;
	}

	const FName Action = Planner->CommitAction(*ExpectedPawn.Get(), GetLane());
	if (Action.IsNone() || Action == NGAIEvents::Failed || !GetDeclaredTransitionEvents().Contains(Action))
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("No executable tactical action"));
		return;
	}
	FinishAction(ENGAIActionResult::Succeeded, Action, Action);
}
