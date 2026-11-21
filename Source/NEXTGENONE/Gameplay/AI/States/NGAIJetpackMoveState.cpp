#include "Gameplay/AI/States/NGAIJetpackMoveState.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "Components/CapsuleComponent.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "Character/ALSBaseCharacter.h"
#include "GameFramework/Character.h"
#include "Gameplay/Character/NGCharacterMovementComponent.h"
#include "Gameplay/Debug/NGAIDebugDraw.h"
#include "GameplayTags/NGGameplayTags.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

namespace
{
	constexpr float MinLandingDistance = 1000.f;
	constexpr float MaxLandingDistance = 6000.f;
	constexpr float LandingUpdateInterval = .25f;
	constexpr float LandingPointTolerance = 45.f;
	constexpr float GroundProbeDepth = 800.f;
}

bool UNGAIJetpackMoveState::ResolveLandingPoint(const FVector& Point, FVector& ProjectedPoint) const
{
	const auto* NavData = LandingNavData.Get();
	FNavLocation NavPoint;
	if (!NavData || !NavData->ProjectPoint(Point, NavPoint, FVector(LandingPointTolerance))) return false;
	const auto& Movement = *FlightMovement.Get();
	const double DistanceSquared = Movement.ProjectToGravityFloor(NavPoint.Location - Movement.GetActorFeetLocation()).SizeSquared();
	if (DistanceSquared < FMath::Square(MinLandingDistance) || DistanceSquared > FMath::Square(MaxLandingDistance)
		|| !IsLandingApproachClear(NavPoint.Location) || !HasLandingRoute(NavPoint.Location)) return false;
	ProjectedPoint = NavPoint.Location;
	return true;
}

bool UNGAIJetpackMoveState::HasLandingRoute(const FVector& Point) const
{
	const auto* NavData = LandingNavData.Get();
	const auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavData || !Navigation) return false;
	FPathFindingQuery Query(ExpectedPawn.Get(), *NavData, LandingNavAnchor, Point);
	return Navigation->TestPathSync(Query.SetAllowPartialPaths(false));
}

bool UNGAIJetpackMoveState::IsLandingApproachClear(const FVector& Point) const
{
	const auto& Movement = *FlightMovement.Get();
	const double Height = Movement.GetGravitySpaceZ(Movement.GetActorFeetLocation() - Point);
	if (Movement.IsFalling() && Height < -LandingPointTolerance) return false;
	const auto* Capsule = Movement.GetCharacterOwner()->GetCapsuleComponent();
	const double ProbeHeight = FMath::Max(Height, 2.0 * LandingPointTolerance);
	const FVector ProbeStart = Point - Movement.GetGravityDirection() * (ProbeHeight + Capsule->GetScaledCapsuleHalfHeight());
	FFindFloorResult Floor;
	Movement.ComputeFloorDist(ProbeStart, 0.f, static_cast<float>(ProbeHeight + LandingPointTolerance), Floor, Capsule->GetScaledCapsuleRadius());
	return Floor.IsWalkableFloor() && FMath::Abs(ProbeHeight - Floor.GetDistanceToFloor()) <= LandingPointTolerance;
}

bool UNGAIJetpackMoveState::FindNearbyLandingPoint(FVector& ProjectedPoint) const
{
	const auto* NavData = LandingNavData.Get();
	if (!NavData) return false;
	const auto* Movement = FlightMovement.Get();
	const FVector Feet = Movement->GetActorFeetLocation();
	const double MinDistanceSquared = FMath::Square(MinLandingDistance);
	double BestDistanceSquared = 0.0;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FNavLocation Candidate;
		if (!NavData->GetRandomReachablePointInRadius(LandingNavAnchor, MaxLandingDistance, Candidate)) continue;
		const double DistanceSquared = Movement->ProjectToGravityFloor(Candidate.Location - Feet).SizeSquared();
		if (DistanceSquared < MinDistanceSquared || DistanceSquared > FMath::Square(MaxLandingDistance)
			|| DistanceSquared <= BestDistanceSquared || !IsLandingApproachClear(Candidate.Location)) continue;
		ProjectedPoint = Candidate.Location;
		BestDistanceSquared = DistanceSquared;
	}
	return BestDistanceSquared > 0.0;
}

bool UNGAIJetpackMoveState::FindLowerLandingPoint(FVector& ProjectedPoint) const
{
	const auto* NavData = LandingNavData.Get();
	if (!NavData) return false;
	const auto& Movement = *FlightMovement.Get();
	const FVector Feet = Movement.GetActorFeetLocation();
	const FVector Gravity = Movement.GetGravityDirection();
	constexpr float MinDrop = 2.f * LandingPointTolerance;
	const FVector DropExtent = Gravity.GetAbs() * ((GroundProbeDepth - MinDrop) * .5f);
	const FVector SearchOrigin = Feet + Gravity * ((GroundProbeDepth + MinDrop) * .5f);
	const FVector SearchExtent = FVector(LandingPointTolerance) + DropExtent;
	const FVector Forward = Movement.RotateGravityToWorld(FVector::ForwardVector);
	const FVector Right = Movement.RotateGravityToWorld(FVector::RightVector);
	const FVector ForwardRight = (Forward + Right).GetSafeNormal();
	const FVector ForwardLeft = (Forward - Right).GetSafeNormal();
	const FVector Directions[] = {Forward, ForwardRight, Right, -ForwardLeft, -Forward, -ForwardRight, -Right, ForwardLeft};
	for (const float Radius : {200.f, 400.f, 600.f, 900.f})
	{
		for (const FVector& Direction : Directions)
		{
			FNavLocation Candidate;
			if (!NavData->ProjectPoint(SearchOrigin + Direction * Radius, Candidate, SearchExtent)) continue;
			const double Drop = FVector::DotProduct(Candidate.Location - Feet, Gravity);
			if (Drop < MinDrop || Drop > GroundProbeDepth || !IsLandingApproachClear(Candidate.Location)) continue;
			ProjectedPoint = Candidate.Location;
			return true;
		}
	}
	return false;
}

void UNGAIJetpackMoveState::RequestLandingQuery()
{
	if (LandingQueryID != INDEX_NONE || IsLandingApproachClear(LandingPoint)) return;
	if (!LandingQuery)
	{
		FindNearbyLandingPoint(LandingPoint);
		return;
	}
	FEnvQueryRequest Request(LandingQuery.Get(), ExpectedPawn.Get());
	const uint64 StartedRun = RunSerial;
	LandingQueryID = Request.Execute(EEnvQueryRunMode::AllMatching, FQueryFinishedSignature::CreateWeakLambda(this,
		[this, StartedRun](TSharedPtr<FEnvQueryResult> QueryResult)
		{
			if (!IsCurrentRun(StartedRun)) return;
			LandingQueryID = INDEX_NONE;
			const auto* Movement = FlightMovement.Get();
			if (!Movement || !Movement->IsFalling() || IsLandingApproachClear(LandingPoint)) return;
			if (!QueryResult || !QueryResult->IsSuccessful() || !QueryResult->ItemType
				|| !QueryResult->ItemType->IsChildOf(UEnvQueryItemType_VectorBase::StaticClass()))
			{
				FindNearbyLandingPoint(LandingPoint);
				return;
			}
			const FVector Feet = Movement->GetActorFeetLocation();
			const double MinDistanceSquared = FMath::Square(MinLandingDistance);
			const double MaxDistanceSquared = FMath::Square(MaxLandingDistance);
			NGAIDebugDraw::DrawLandingCandidates(*Movement, *QueryResult, MinDistanceSquared, MaxDistanceSquared, LandingUpdateInterval);
			double BestDistanceSquared = 0.0;
			for (int32 Index = 0; Index < QueryResult->Items.Num(); ++Index)
			{
				const FVector Candidate = QueryResult->GetItemAsLocation(Index);
				if (Candidate.ContainsNaN()) continue;
				const double DistanceSquared = Movement->ProjectToGravityFloor(Candidate - Feet).SizeSquared();
				if (DistanceSquared < MinDistanceSquared || DistanceSquared > MaxDistanceSquared) continue;
				FVector ProjectedPoint;
				if (!ResolveLandingPoint(Candidate, ProjectedPoint)) continue;
				const double ProjectedDistanceSquared = Movement->ProjectToGravityFloor(ProjectedPoint - Feet).SizeSquared();
				if (ProjectedDistanceSquared <= BestDistanceSquared) continue;
				LandingPoint = ProjectedPoint;
				BestDistanceSquared = ProjectedDistanceSquared;
			}
			if (BestDistanceSquared == 0.0) FindNearbyLandingPoint(LandingPoint);
		}));
	if (LandingQueryID == INDEX_NONE) FindNearbyLandingPoint(LandingPoint);
}

void UNGAIJetpackMoveState::StartAction()
{
	bRestoreRotationMode = false;
	bRecoveryFlight = false;
	auto* Character = Cast<ACharacter>(ExpectedPawn.Get());
	auto* Movement = Character ? Cast<UNGCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!Movement || (!Movement->IsMovingOnGround() && !Movement->IsFalling()) || Destination.ContainsNaN()
		|| !FMath::IsFinite(ThrustDuration) || ThrustDuration <= 0.f || ThrustDuration >= ActionTimeout
		|| !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 1.f)
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("Cannot start jetpack move"));
		return;
	}
	FlightMovement = Movement;
	const auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const auto* NavData = Navigation ? Navigation->GetNavDataForProps(Character->GetNavAgentPropertiesRef(), Character->GetActorLocation()) : nullptr;
	FVector LaunchSurface = Movement->GetActorFeetLocation();
	if (Movement->IsFalling())
	{
		const auto* Capsule = Character->GetCapsuleComponent();
		FFindFloorResult Floor;
		Movement->ComputeFloorDist(Capsule->GetComponentLocation(), 0.f, GroundProbeDepth, Floor, Capsule->GetScaledCapsuleRadius());
		if (!Floor.IsWalkableFloor())
		{
			FinishAction(ENGAIActionResult::Failed, TEXT("No walkable floor below flight start"));
			return;
		}
		LaunchSurface += Movement->GetGravityDirection() * Floor.GetDistanceToFloor();
	}
	FNavLocation LaunchNavPoint;
	if (!NavData)
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("No navigation data for flight"));
		return;
	}
	LandingNavData = NavData;
	bool bFoundLanding = false;
	if (NavData->ProjectPoint(LaunchSurface, LaunchNavPoint, FVector(LandingPointTolerance)))
	{
		LandingNavAnchor = LaunchNavPoint.Location;
		const FVector RequestedOffset = Movement->ProjectToGravityFloor(Destination - Movement->GetActorFeetLocation());
		const FVector RequestedLanding = Destination - RequestedOffset + RequestedOffset.GetClampedToMaxSize(MaxLandingDistance);
		bFoundLanding = (!Destination.IsZero() && ResolveLandingPoint(RequestedLanding, LandingPoint)) || FindNearbyLandingPoint(LandingPoint);
	}
	if (!bFoundLanding)
	{
		if (!Movement->IsMovingOnGround() || !FindLowerLandingPoint(LandingPoint))
		{
			FinishAction(ENGAIActionResult::Failed, TEXT("No reachable or lower landing point"));
			return;
		}
		LandingNavAnchor = LandingPoint;
		bRecoveryFlight = true;
	}
	bWasAirborne = Movement->IsFalling();
	bBrakeForEdge = false;
	LandingUpdateRemaining = 0.f;
	if (auto* ALSCharacter = Cast<AALSBaseCharacter>(ExpectedPawn.Get()))
	{
		PreviousRotationMode = ALSCharacter->GetRotationMode();
		bRestoreRotationMode = PreviousRotationMode != EALSRotationMode::VelocityDirection;
		ALSCharacter->SetRotationMode(EALSRotationMode::VelocityDirection);
	}
	const uint64 StartedRun = RunSerial;
	if (!JetpackInput.Start(this, ActionASC.Get(), TAG_NGInputJetpack, ThrustDuration,
		[this, StartedRun]() { return IsCurrentRun(StartedRun); },
		[this]() { return HasCurrentAvatar(); },
		[this](bool bCancelled)
		{
			if (bCancelled || !bWasAirborne) FinishAction(ENGAIActionResult::Failed, TEXT("Jetpack ended before flight completed"));
		}))
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("Jetpack thrust rejected"));
		return;
	}
	if (IsCurrentRun(StartedRun)) Movement->SetFlightControl(this, SteeringAirControl, ThrustScale);
}

void UNGAIJetpackMoveState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsCurrentRun(RunSerial)) return;

	auto* Movement = FlightMovement.Get();
	if (!Movement)
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("Flight movement unavailable"));
		return;
	}

	const FVector Feet = Movement->GetActorFeetLocation();
	const FVector LandingOffset = LandingPoint - Feet;
	const FVector HorizontalOffset = Movement->ProjectToGravityFloor(LandingOffset);
	if (Movement->IsMovingOnGround() && bWasAirborne)
	{
		const auto* NavData = LandingNavData.Get();
		FNavLocation ActualLanding;
		const bool bReachedDestination = HorizontalOffset.SizeSquared() <= FMath::Square(AcceptanceRadius)
			&& FMath::Abs(Movement->GetGravitySpaceZ(LandingOffset)) <= LandingPointTolerance
			&& NavData && NavData->ProjectPoint(Feet, ActualLanding, FVector(LandingPointTolerance))
			&& HasLandingRoute(ActualLanding.Location);
		FinishAction(bReachedDestination ? ENGAIActionResult::Succeeded : ENGAIActionResult::Failed,
			bReachedDestination ? TEXT("Landed at destination") : TEXT("Landed outside destination"));
		return;
	}
	if (!Movement->IsFalling() && !Movement->IsMovingOnGround())
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("Flight movement interrupted"));
		return;
	}

	bWasAirborne |= Movement->IsFalling();
	const FVector HorizontalVelocity = Movement->ProjectToGravityFloor(Movement->Velocity);
	LandingUpdateRemaining -= DeltaTime;
	if (!bRecoveryFlight && bWasAirborne && LandingUpdateRemaining <= 0.f)
	{
		LandingUpdateRemaining = LandingUpdateInterval;
		const auto* NavData = LandingNavData.Get();
		FNavLocation ProjectedPoint;
		bBrakeForEdge = HorizontalVelocity.SizeSquared() > FMath::Square(100.f)
			&& (!NavData || !NavData->ProjectPoint(Feet + HorizontalVelocity * .65f, ProjectedPoint,
				FVector(LandingPointTolerance, LandingPointTolerance, GroundProbeDepth))
				|| Movement->GetGravitySpaceZ(ProjectedPoint.Location - Feet) > 60.f);
		RequestLandingQuery();
	}
	NGAIDebugDraw::DrawJetpackFlight(*Movement, LandingPoint, bBrakeForEdge);
	constexpr double FullInputVelocityError = 120.0;
	const double RemainingDistance = FMath::Max(0.0, HorizontalOffset.Size() - AcceptanceRadius * .25);
	const double ApproachSpeed = FMath::Min<double>(Movement->GetMaxSpeed(), FMath::Sqrt(Movement->GetMaxAcceleration() * RemainingDistance));
	const FVector DesiredVelocity = bBrakeForEdge ? FVector::ZeroVector : HorizontalOffset.GetSafeNormal() * ApproachSpeed;
	const FVector Steering = (DesiredVelocity - HorizontalVelocity).GetClampedToMaxSize(FullInputVelocityError) / FullInputVelocityError;
	ExpectedPawn->AddMovementInput(Steering.GetSafeNormal(), Steering.Size());
}

void UNGAIJetpackMoveState::ReleaseActionResources()
{
	const int32 PendingQuery = LandingQueryID;
	LandingQueryID = INDEX_NONE;
	if (PendingQuery != INDEX_NONE)
		if (auto* Manager = UEnvQueryManager::GetCurrent(GetWorld())) Manager->AbortQuery(PendingQuery);
	JetpackInput.Reset();
	if (auto* Movement = FlightMovement.Get()) Movement->ClearFlightControl(this);
	if (bRestoreRotationMode)
	{
		if (auto* ALSCharacter = Cast<AALSBaseCharacter>(ExpectedPawn.Get());
			ALSCharacter && ALSCharacter->GetRotationMode() == EALSRotationMode::VelocityDirection)
			ALSCharacter->SetRotationMode(PreviousRotationMode);
		bRestoreRotationMode = false;
	}
	FlightMovement.Reset();
	LandingNavData.Reset();
}
