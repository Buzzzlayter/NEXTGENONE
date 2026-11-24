#pragma once

#include "Gameplay/AI/Planning/NGAIPlanningProfile.h"

/** Unknown is separate from false. Facts are a projection of gameplay, never its owner. */
struct FNGAIFacts
{
	uint64 Known = 0;
	uint64 Values = 0;
	void Set(ENGAIFact Fact, bool Value);
	bool Matches(const TArray<FNGAIFactValue>& Conditions) const;
};

struct FNGAIDecisionSnapshot
{
	FNGAIFacts Facts;
	TMap<FName, float> Inputs;
};

struct FNGAIPlan
{
	ENGAIPlanStatus Status = ENGAIPlanStatus::NoPlan;
	TArray<FName> Actions;
	float Cost = 0.f;
	int32 ExpandedNodes = 0;
};

namespace NGAIGOAP
{
	NEXTGENONE_API FNGAIPlan BuildPlan(
		const UNGAIPlanningProfile& Profile, const FNGAIDecisionSnapshot& Snapshot, FName GoalId);
}
