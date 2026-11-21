#include "Gameplay/AI/States/NGAIJetpackMoveState.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "DrawDebugHelpers.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "GameFramework/Character.h"
#include "Gameplay/Character/NGCharacterMovementComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"

namespace
{
	constexpr float MinLandingDistance = 100.f;
	constexpr float MaxLandingDistance = 650.f;
	constexpr float LandingUpdateInterval = .25f;
	constexpr float LandingPointTolerance = 45.f;

#if WITH_EDITOR && ENABLE_DRAW_DEBUG
	bool IsJetpackDebugEnabled()
	{
		static TAutoConsoleVariable<bool> DrawJetpackFlight(TEXT("ng.AI.DebugJetpack"), true, TEXT("Draw AI jetpack landing targets, velocity and EQS candidates."));
		return DrawJetpackFlight.GetValueOnGameThread();
	}
#endif
}

bool UNGAIJetpackMoveState::ProjectLandingPoint(const FVector& Point, FVector& ProjectedPoint) const
{
	auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation) return false;
	const auto* Pawn = ExpectedPawn.Get();
	const auto* NavData = Navigation->GetNavDataForProps(Pawn->GetNavAgentPropertiesRef(), Pawn->GetActorLocation());
	FNavLocation NavPoint;
	if (!NavData || !Navigation->ProjectPointToNavigation(Point, NavPoint, FVector(45.f, 45.f, 800.f), NavData)) return false;
	const auto* Movement = FlightMovement.Get();
	const double Depth = FVector::DotProduct(NavPoint.Location - Movement->GetActorFeetLocation(), Movement->GetGravityDirection());
	if (Movement->IsFalling() && Depth < -60.f) return false;
	ProjectedPoint = NavPoint.Location;
	return true;
}

bool UNGAIJetpackMoveState::FindNearbyLandingPoint(FVector& ProjectedPoint) const
{
	auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation) return false;
	const auto* Pawn = ExpectedPawn.Get();
	const auto* Movement = FlightMovement.Get();
	const FVector Feet = Movement->GetActorFeetLocation();
	auto* NavData = Navigation->GetNavDataForProps(Pawn->GetNavAgentPropertiesRef(), Feet);
	if (!NavData) return false;
	const double MinDistanceSquared = FMath::Square(MinLandingDistance);
	double BestDistanceSquared = 0.0;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FNavLocation Candidate;
		if (!Navigation->GetRandomReachablePointInRadius(Feet, MaxLandingDistance, Candidate, NavData)) continue;
		const FVector LandingOffset = Candidate.Location - Feet;
		const double DistanceSquared = Movement->ProjectToGravityFloor(LandingOffset).SizeSquared();
		if (DistanceSquared < MinDistanceSquared || DistanceSquared <= BestDistanceSquared) continue;
		if (Movement->IsFalling() && FVector::DotProduct(LandingOffset, Movement->GetGravityDirection()) < -60.f) continue;
		ProjectedPoint = Candidate.Location;
		BestDistanceSquared = DistanceSquared;
	}
	return BestDistanceSquared > 0.0;
}

void UNGAIJetpackMoveState::RequestLandingQuery()
{
	if (!LandingQuery || LandingQueryID != INDEX_NONE) return;
	FEnvQueryRequest Request(LandingQuery.Get(), ExpectedPawn.Get());
	const uint64 StartedRun = RunSerial;
	LandingQueryID = Request.Execute(EEnvQueryRunMode::AllMatching, FQueryFinishedSignature::CreateWeakLambda(this,
		[this, StartedRun](TSharedPtr<FEnvQueryResult> QueryResult)
		{
			if (!IsCurrentRun(StartedRun)) return;
			LandingQueryID = INDEX_NONE;
			const auto* Movement = FlightMovement.Get();
			if (!Movement || !Movement->IsFalling() || !QueryResult || !QueryResult->IsSuccessful() || !QueryResult->ItemType
				|| !QueryResult->ItemType->IsChildOf(UEnvQueryItemType_VectorBase::StaticClass())) return;
			const FVector Feet = Movement->GetActorFeetLocation();
			const double MinDistanceSquared = FMath::Square(MinLandingDistance);
			const double MaxDistanceSquared = FMath::Square(MaxLandingDistance);
#if WITH_EDITOR && ENABLE_DRAW_DEBUG
			if (IsJetpackDebugEnabled())
			{
				for (int32 Index = 0; Index < QueryResult->Items.Num(); ++Index)
				{
					const FVector Candidate = QueryResult->GetItemAsLocation(Index);
					if (Candidate.ContainsNaN()) continue;
					const double DistanceSquared = Movement->ProjectToGravityFloor(Candidate - Feet).SizeSquared();
					const FColor CandidateColor = DistanceSquared < MinDistanceSquared ? FColor::Red
						: DistanceSquared > MaxDistanceSquared ? FColor::Silver : FColor::Purple;
					DrawDebugPoint(GetWorld(), Candidate - Movement->GetGravityDirection() * 12.f, 9.f, CandidateColor, false, LandingUpdateInterval);
				}
			}
#endif
			FVector ProjectedLandingPoint;
			if (ProjectLandingPoint(LandingPoint, ProjectedLandingPoint) && ProjectedLandingPoint.Equals(LandingPoint, LandingPointTolerance)) return;
			double BestDistanceSquared = 0.0;
			for (int32 Index = 0; Index < QueryResult->Items.Num(); ++Index)
			{
				const FVector Candidate = QueryResult->GetItemAsLocation(Index);
				if (Candidate.ContainsNaN()) continue;
				const double DistanceSquared = Movement->ProjectToGravityFloor(Candidate - Feet).SizeSquared();
				if (DistanceSquared < MinDistanceSquared || DistanceSquared > MaxDistanceSquared) continue;
				FVector ProjectedPoint;
				if (!ProjectLandingPoint(Candidate, ProjectedPoint)) continue;
				const double ProjectedDistanceSquared = Movement->ProjectToGravityFloor(ProjectedPoint - Feet).SizeSquared();
				if (ProjectedDistanceSquared < MinDistanceSquared || ProjectedDistanceSquared > MaxDistanceSquared
					|| ProjectedDistanceSquared <= BestDistanceSquared) continue;
				LandingPoint = ProjectedPoint;
				BestDistanceSquared = ProjectedDistanceSquared;
			}
		}));
}

void UNGAIJetpackMoveState::StartAction()
{
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
	const FVector Feet = Movement->GetActorFeetLocation();
	bool bFoundLanding = false;
	if (!Destination.IsZero())
	{
		const FVector InitialOffset = Movement->ProjectToGravityFloor(Destination - Feet);
		const bool bShortenFlight = InitialOffset.SizeSquared() > FMath::Square(MaxLandingDistance);
		bFoundLanding = (bShortenFlight && ProjectLandingPoint(Feet + InitialOffset.GetClampedToMaxSize(MaxLandingDistance), LandingPoint))
			|| ProjectLandingPoint(Destination, LandingPoint);
	}
	if (bFoundLanding && Movement->ProjectToGravityFloor(LandingPoint - Feet).SizeSquared() < FMath::Square(MinLandingDistance))
		bFoundLanding = false;
	if (!bFoundLanding && !FindNearbyLandingPoint(LandingPoint))
	{
		FinishAction(ENGAIActionResult::Failed, TEXT("No navigable landing point at minimum flight distance"));
		return;
	}
	bWasAirborne = Movement->IsFalling();
	bBrakeForEdge = false;
	LandingUpdateRemaining = 0.f;
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
	const FVector HorizontalOffset = Movement->ProjectToGravityFloor(LandingPoint - Feet);
	if (Movement->IsMovingOnGround() && bWasAirborne)
	{
		const bool bReachedDestination = HorizontalOffset.SizeSquared() <= FMath::Square(AcceptanceRadius);
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
	if (bWasAirborne && LandingUpdateRemaining <= 0.f)
	{
		LandingUpdateRemaining = LandingUpdateInterval;
		FVector ProjectedPoint;
		bBrakeForEdge = HorizontalVelocity.SizeSquared() > FMath::Square(100.f)
			&& !ProjectLandingPoint(Feet + HorizontalVelocity * .65f, ProjectedPoint);
		RequestLandingQuery();
	}
#if WITH_EDITOR && ENABLE_DRAW_DEBUG
	if (IsJetpackDebugEnabled())
	{
		const FVector ArrowOrigin = Feet - Movement->GetGravityDirection() * 75.f;
		const FColor LandingColor = bBrakeForEdge ? FColor::Yellow : FColor::Cyan;
		const UWorld* World = GetWorld();
		DrawDebugDirectionalArrow(World, ArrowOrigin, LandingPoint, 22.f, LandingColor, false, 0.f, 0, 2.f);
		DrawDebugSphere(World, LandingPoint, 20.f, 12, LandingColor, false, 0.f, 0, 2.f);
		if (!Movement->Velocity.IsNearlyZero())
			DrawDebugDirectionalArrow(World, ArrowOrigin, ArrowOrigin + Movement->Velocity * .35f, 18.f, FColor::Green, false, 0.f, 0, 2.f);
	}
#endif
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
	FlightMovement.Reset();
}
