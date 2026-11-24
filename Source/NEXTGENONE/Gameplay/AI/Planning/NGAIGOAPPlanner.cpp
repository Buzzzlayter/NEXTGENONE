#include "Gameplay/AI/Planning/NGAIGOAPPlanner.h"

namespace
{
	bool ValidFacts(const TArray<FNGAIFactValue>& Facts)
	{
		uint64 Seen = 0;
		for (const auto& Entry : Facts)
		{
			if (Entry.Fact >= ENGAIFact::Count) return false;
			const uint64 Bit = uint64(1) << static_cast<uint8>(Entry.Fact);
			if (Seen & Bit) return false;
			Seen |= Bit;
		}
		return true;
	}

	bool ValidProfile(const UNGAIPlanningProfile& Profile)
	{
		if (Profile.MaxPlanDepth < 1 || Profile.MaxPlanDepth > 64 || Profile.MaxSearchNodes < 1
			|| Profile.MaxSearchNodes > 4096 || Profile.Actions.Num() > 64 || Profile.Goals.Num() > 32) return false;
		TSet<FName> Ids;
		for (const auto& Action : Profile.Actions)
		{
			if (Action.Id.IsNone() || Ids.Contains(Action.Id) || Action.Effects.IsEmpty()
				|| !FMath::IsFinite(Action.BaseCost) || Action.BaseCost < 0.f
				|| !ValidFacts(Action.Preconditions) || !ValidFacts(Action.Effects)) return false;
			Ids.Add(Action.Id);
		}
		Ids.Reset();
		for (const auto& Goal : Profile.Goals)
		{
			if (Goal.Id.IsNone() || Ids.Contains(Goal.Id) || Goal.DesiredState.IsEmpty()
				|| !FMath::IsFinite(Goal.Priority) || Goal.Priority < 0.f
				|| !ValidFacts(Goal.Eligibility) || !ValidFacts(Goal.DesiredState)) return false;
			Ids.Add(Goal.Id);
		}
		return true;
	}

	bool Evaluate(const FNGAIScoreTerm& Term, const FNGAIDecisionSnapshot& Snapshot, float& Value)
	{
		const float* Input = Snapshot.Inputs.Find(Term.Input);
		const auto* Curve = Term.Curve.GetRichCurveConst();
		if (!Input || !FMath::IsFinite(*Input) || !Curve || Curve->GetNumKeys() == 0) return false;
		Value = Curve->Eval(FMath::Clamp(*Input, 0.f, 1.f));
		return FMath::IsFinite(Value) && Value >= 0.f;
	}
}

void FNGAIFacts::Set(ENGAIFact Fact, bool Value)
{
	if (Fact >= ENGAIFact::Count) return;
	const uint64 Bit = uint64(1) << static_cast<uint8>(Fact);
	Known |= Bit;
	Values = Value ? Values | Bit : Values & ~Bit;
}

bool FNGAIFacts::Matches(const TArray<FNGAIFactValue>& Conditions) const
{
	for (const auto& Entry : Conditions)
	{
		if (Entry.Fact >= ENGAIFact::Count) return false;
		const uint64 Bit = uint64(1) << static_cast<uint8>(Entry.Fact);
		if (!(Known & Bit) || bool(Values & Bit) != Entry.Value) return false;
	}
	return true;
}

FNGAIPlan NGAIGOAP::BuildPlan(const UNGAIPlanningProfile& Profile, const FNGAIDecisionSnapshot& Snapshot, FName GoalId)
{
	FNGAIPlan Plan;
	const auto* Goal = Profile.Goals.FindByPredicate([GoalId](const FNGAIPlanningGoal& Entry) { return Entry.Id == GoalId; });
	if (!ValidProfile(Profile) || !Goal)
	{ Plan.Status = ENGAIPlanStatus::InvalidConfiguration; return Plan; }
	if (!Snapshot.Facts.Matches(Goal->Eligibility)) return Plan;
	TArray<float> Costs;
	for (const auto& Action : Profile.Actions)
	{
		float Cost = Action.BaseCost;
		for (const auto& Term : Action.CostTerms)
		{
			float Value;
			if (!Evaluate(Term, Snapshot, Value)) { Cost = -1.f; break; }
			Cost += Value;
		}
		Costs.Add(FMath::IsFinite(Cost) ? Cost : -1.f);
	}
	struct FNode
	{
		FNGAIFacts Facts;
		float Cost;
		int32 Parent, Action, Depth;
		bool Closed = false;
	};
	TArray<FNode> Nodes;
	Nodes.Add({Snapshot.Facts, 0.f, INDEX_NONE, INDEX_NONE, 0});
	bool bLimited = false;
	while (true)
	{
		int32 Best = INDEX_NONE;
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
			if (!Nodes[Index].Closed && (Best == INDEX_NONE || Nodes[Index].Cost < Nodes[Best].Cost)) Best = Index;
		if (Best == INDEX_NONE) break;
		Nodes[Best].Closed = true;
		const FNode Current = Nodes[Best]; // Adding successors may reallocate Nodes.
		++Plan.ExpandedNodes;
		if (Current.Facts.Matches(Goal->DesiredState))
		{
			Plan.Status = ENGAIPlanStatus::Succeeded;
			Plan.Cost = Current.Cost;
			for (int32 Index = Best; Nodes[Index].Parent != INDEX_NONE; Index = Nodes[Index].Parent)
				Plan.Actions.Insert(Profile.Actions[Nodes[Index].Action].Id, 0);
			return Plan;
		}
		if (Current.Depth >= Profile.MaxPlanDepth) { bLimited = true; continue; }
		for (int32 ActionIndex = 0; ActionIndex < Profile.Actions.Num(); ++ActionIndex)
		{
			const auto& Action = Profile.Actions[ActionIndex];
			if (Costs[ActionIndex] < 0.f || !Current.Facts.Matches(Action.Preconditions)) continue;
			FNode Next {Current.Facts, Current.Cost + Costs[ActionIndex], Best, ActionIndex, Current.Depth + 1};
			if (!FMath::IsFinite(Next.Cost)) continue;
			for (const auto& Effect : Action.Effects) Next.Facts.Set(Effect.Fact, Effect.Value);
			const bool bDominated = Nodes.ContainsByPredicate([&Next](const FNode& Existing)
			{
				return Existing.Facts.Known == Next.Facts.Known && Existing.Facts.Values == Next.Facts.Values
					&& Existing.Cost <= Next.Cost && Existing.Depth <= Next.Depth;
			});
			if (bDominated) continue;
			if (Nodes.Num() >= Profile.MaxSearchNodes) { bLimited = true; continue; }
			Nodes.Add(Next);
		}
	}
	Plan.Status = bLimited ? ENGAIPlanStatus::BudgetExceeded : ENGAIPlanStatus::NoPlan;
	return Plan;
}
