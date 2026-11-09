#include "Gameplay/AI/NGAIMoveState.h"

#include "Gameplay/AI/NGAIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"

void UNGAIMoveState::StartAction()
{
	if (Destination.ContainsNaN() || !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 0.f)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Invalid move configuration")); return; }
	const FVector Goal = bRelativeToPawn ? ExpectedPawn->GetActorTransform().TransformPosition(Destination) : Destination;
	FAIMoveRequest Request(Goal);
	Request.SetAcceptanceRadius(AcceptanceRadius);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(false);
	Request.SetProjectGoalLocation(true);
	const auto MoveResult = ActionController->MoveTo(Request);
	if (!bRunning) return;
	if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{ FinishAction(ENGAIActionResult::Succeeded, TEXT("Already at goal")); return; }
	if (MoveResult.Code != EPathFollowingRequestResult::RequestSuccessful)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Move request rejected")); return; }
	MoveRequestID = MoveResult.MoveId;
	PathFollowing = ActionController->GetPathFollowingComponent();
	if (!PathFollowing.IsValid()) { FinishAction(ENGAIActionResult::Failed, TEXT("No path following")); return; }
	const uint64 StartedRun = RunSerial;
	MoveFinishedHandle = PathFollowing->OnRequestFinished.AddWeakLambda(this, [this, StartedRun](FAIRequestID RequestID, const FPathFollowingResult& PathResult)
	{
		if (!IsCurrentRun(StartedRun) || RequestID != MoveRequestID) return;
		const auto Outcome = PathResult.Code == EPathFollowingResult::Success ? ENGAIActionResult::Succeeded
			: PathResult.Code == EPathFollowingResult::Aborted ? ENGAIActionResult::Aborted : ENGAIActionResult::Failed;
		FinishAction(Outcome, TEXT("Path following completed"));
	});
}

void UNGAIMoveState::ReleaseActionResources()
{
	if (auto* Following = PathFollowing.Get())
	{
		Following->OnRequestFinished.Remove(MoveFinishedHandle);
		if (MoveRequestID.IsValid() && Following->GetCurrentRequestId() == MoveRequestID)
			Following->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
	}
	PathFollowing.Reset(); MoveFinishedHandle.Reset(); MoveRequestID = FAIRequestID::InvalidRequest;
}
