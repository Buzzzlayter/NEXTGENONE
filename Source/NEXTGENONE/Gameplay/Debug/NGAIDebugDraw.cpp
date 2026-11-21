#include "Gameplay/Debug/NGAIDebugDraw.h"

#include "DrawDebugHelpers.h"

#if WITH_EDITOR && ENABLE_DRAW_DEBUG
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDebugJetpack(TEXT("ng.AI.DebugJetpack"), false,
		TEXT("Draw AI jetpack landing targets, velocity and EQS candidates."));
}
#endif

void NGAIDebugDraw::DrawJetpackFlight(const UCharacterMovementComponent& Movement, const FVector& LandingPoint, bool bBraking)
{
#if WITH_EDITOR && ENABLE_DRAW_DEBUG
	if (!CVarDebugJetpack.GetValueOnGameThread()) return;

	const UWorld* World = Movement.GetWorld();
	const FVector ArrowOrigin = Movement.GetActorFeetLocation() - Movement.GetGravityDirection() * 75.f;
	const FColor LandingColor = bBraking ? FColor::Yellow : FColor::Cyan;
	DrawDebugDirectionalArrow(World, ArrowOrigin, LandingPoint, 22.f, LandingColor, false, 0.f, 0, 2.f);
	DrawDebugSphere(World, LandingPoint, 20.f, 12, LandingColor, false, 0.f, 0, 2.f);
	if (!Movement.Velocity.IsNearlyZero())
		DrawDebugDirectionalArrow(World, ArrowOrigin, ArrowOrigin + Movement.Velocity * .35f, 18.f, FColor::Green, false, 0.f, 0, 2.f);
#endif
}

void NGAIDebugDraw::DrawLandingCandidates(const UCharacterMovementComponent& Movement, const FEnvQueryResult& QueryResult,
	double MinDistanceSquared, double MaxDistanceSquared, float DrawDuration)
{
#if WITH_EDITOR && ENABLE_DRAW_DEBUG
	if (!CVarDebugJetpack.GetValueOnGameThread()) return;

	const UWorld* World = Movement.GetWorld();
	const FVector Feet = Movement.GetActorFeetLocation();
	const FVector PointOffset = -Movement.GetGravityDirection() * 12.f;
	for (int32 Index = 0; Index < QueryResult.Items.Num(); ++Index)
	{
		const FVector Candidate = QueryResult.GetItemAsLocation(Index);
		if (Candidate.ContainsNaN()) continue;
		const double DistanceSquared = Movement.ProjectToGravityFloor(Candidate - Feet).SizeSquared();
		const FColor CandidateColor = DistanceSquared < MinDistanceSquared ? FColor::Red
			: DistanceSquared > MaxDistanceSquared ? FColor::Silver : FColor::Purple;
		DrawDebugPoint(World, Candidate + PointOffset, 9.f, CandidateColor, false, DrawDuration);
	}
#endif
}
