#include "Gameplay/AI/States/NGAIMoveState.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"

bool UNGAIMoveState::ResolveDestination(FVector& Goal) const
{
	if (DestinationType == ENGAIMoveDestination::Point)
	{
		Goal = bRelativeToPawn ? ExpectedPawn->GetActorTransform().TransformPosition(Destination) : Destination;
		return !Goal.ContainsNaN();
	}
	const auto* Target = ActionController->GetMemory()->GetTarget();
	if (!Target) return false;
	if (DestinationType == ENGAIMoveDestination::LastKnownTarget)
	{
		Goal = Target->GetKnownLocation();
		return !Goal.ContainsNaN();
	}
	auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (DestinationType != ENGAIMoveDestination::Reposition || !Navigation
		|| !FMath::IsFinite(MinTargetDistance) || MinTargetDistance < 100.f
		|| !FMath::IsFinite(MaxTargetDistance) || MaxTargetDistance < MinTargetDistance
		|| !FMath::IsFinite(MinMoveDistance) || MinMoveDistance < 0.f) return false;
	const FVector Center = Target->GetKnownLocation();
	const auto* NavData = Navigation->GetNavDataForProps(ExpectedPawn->GetNavAgentPropertiesRef(), ExpectedPawn->GetActorLocation());
	if (!NavData) return false;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		const float Angle = FMath::FRandRange(-PI, PI);
		const float Radius = FMath::FRandRange(MinTargetDistance, MaxTargetDistance);
		const FVector Candidate = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
		FNavLocation Projected;
		if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(100.f, 100.f, 500.f), NavData)) continue;
		const float TargetDistance = FVector::Dist2D(Center, Projected.Location);
		if (TargetDistance < MinTargetDistance || TargetDistance > MaxTargetDistance
			|| FVector::Dist2D(ExpectedPawn->GetActorLocation(), Projected.Location) < MinMoveDistance) continue;
		Goal = Projected.Location;
		return true;
	}
	return false;
}

void UNGAIMoveState::StartAction()
{
	if ((DestinationType == ENGAIMoveDestination::Point && Destination.ContainsNaN())
		|| !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 0.f)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Invalid move configuration")); return; }
	FVector Goal;
	if (!ResolveDestination(Goal))
	{ FinishAction(ENGAIActionResult::Failed, TEXT("No move destination")); return; }
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
