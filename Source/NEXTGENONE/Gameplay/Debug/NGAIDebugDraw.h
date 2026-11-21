#pragma once

#include "Math/Vector.h"

class UCharacterMovementComponent;
struct FEnvQueryResult;

namespace NGAIDebugDraw
{
	void DrawJetpackFlight(const UCharacterMovementComponent& Movement, const FVector& LandingPoint, bool bBraking);
	void DrawLandingCandidates(const UCharacterMovementComponent& Movement, const FEnvQueryResult& QueryResult,
		double MinDistanceSquared, double MaxDistanceSquared, float DrawDuration);
}
