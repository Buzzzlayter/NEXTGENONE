#include "Gameplay/AI/Components/NGAIGOAPComponent.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"
#include "Gameplay/AI/Components/NGAIQueryResultsComponent.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Inventory/NGInventoryItemDefinition.h"

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

void FNGAIFacts::Forget(ENGAIFact Fact)
{
	if (Fact >= ENGAIFact::Count) return;
	const uint64 Bit = uint64(1) << static_cast<uint8>(Fact);
	Known &= ~Bit;
	Values &= ~Bit;
}

static FName SelectGoal(const UNGAIPlanningProfile& Profile, const FNGAIDecisionSnapshot& Snapshot,
	FName CurrentGoal, float SwitchMargin)
{
	if (!ValidProfile(Profile) || !FMath::IsFinite(SwitchMargin) || SwitchMargin < 0.f) return NAME_None;
	FName Best;
	float BestScore = 0.f;
	float CurrentScore = 0.f;
	for (const auto& Goal : Profile.Goals)
	{
		if (!Snapshot.Facts.Matches(Goal.Eligibility) || Snapshot.Facts.Matches(Goal.DesiredState)) continue;
		float Score = Goal.Priority;
		for (const auto& Term : Goal.UtilityTerms)
		{
			float Value;
			if (!Evaluate(Term, Snapshot, Value)) { Score = 0.f; break; }
			Score *= FMath::Clamp(Value, 0.f, 1.f);
		}
		if (Goal.Id == CurrentGoal) CurrentScore = Score;
		if (Score > BestScore || (Score == BestScore && Score > 0.f && Goal.Id == CurrentGoal))
		{ Best = Goal.Id; BestScore = Score; }
	}
	return CurrentScore > 0.f && BestScore <= CurrentScore + SwitchMargin ? CurrentGoal : Best;
}

static FNGAIPlan BuildPlan(const UNGAIPlanningProfile& Profile, const FNGAIDecisionSnapshot& Snapshot, FName GoalId)
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

void UNGAIGOAPComponent::Enter()
{
	ClearSnapshot();
	Controller = Cast<ANGAIController>(GetContextOwner());
	auto* AI = Controller.Get();
	Pawn = AI ? AI->GetPawn() : nullptr;
	const auto* ASC = AI ? Cast<UNGPlayerAbilitySystemComponent>(AI->GetReadyAbilitySystem(Pawn.Get())) : nullptr;
	AvatarGeneration = ASC ? ASC->GetAvatarGeneration() : 0;
	if (!ASC) Pawn.Reset();
	Super::Enter();
}

void UNGAIGOAPComponent::Exit(FName Event)
{
	Pawn.Reset();
	Controller.Reset();
	ClearSnapshot();
	Super::Exit(Event);
}

void UNGAIGOAPComponent::ClearSnapshot()
{
	Snapshot = FNGAIDecisionSnapshot();
	Plan = FNGAIPlan();
	CurrentGoal = NAME_None;
	NextRetryTime = 0.;
	DecisionTarget.Reset();
	LastReason = NAME_None;
}

void UNGAIGOAPComponent::SetFact(ENGAIFact Fact, bool Value)
{
	Snapshot.Facts.Set(Fact, Value);
	InvalidatePlan(TEXT("Facts edited"));
}

void UNGAIGOAPComponent::SetInput(FName Input, float Value)
{
	if (!Input.IsNone() && FMath::IsFinite(Value)) Snapshot.Inputs.Add(Input, Value);
	else Snapshot.Inputs.Remove(Input);
	InvalidatePlan(TEXT("Inputs edited"));
}

bool UNGAIGOAPComponent::Replan()
{
	if (GetWorld() && GetWorld()->GetTimeSeconds() < NextRetryTime) return false;
	if (!RefreshFacts()) return false;
	Plan = FNGAIPlan();
	auto* AI = Controller.Get();
	const auto* ASC = AI && Pawn.IsValid() && AI->GetPawn() == Pawn.Get()
		? Cast<UNGPlayerAbilitySystemComponent>(AI->GetReadyAbilitySystem(Pawn.Get())) : nullptr;
	if (!ASC || ASC->GetAvatarGeneration() != AvatarGeneration || !Profile)
	{
		CurrentGoal = NAME_None;
		Plan.Status = ENGAIPlanStatus::InvalidConfiguration;
		return false;
	}
	CurrentGoal = SelectGoal(*Profile, Snapshot, CurrentGoal, SwitchMargin);
	if (CurrentGoal.IsNone()) { InvalidatePlan(TEXT("No eligible goal or invalid profile")); return false; }
	Plan = BuildPlan(*Profile, Snapshot, CurrentGoal);
	if (Plan.Status != ENGAIPlanStatus::Succeeded)
	{
		LastReason = TEXT("No executable plan; inspect PlanStatus");
		NextRetryTime = GetWorld()->GetTimeSeconds() + (FMath::IsFinite(RetryDelay) ? FMath::Max(.1f, RetryDelay) : .5f);
		return false;
	}
	LastReason = TEXT("Plan ready");
	return true;
}

bool UNGAIGOAPComponent::HasCurrentLife() const
{
	auto* AI = Controller.Get();
	const auto* ASC = AI && Pawn.IsValid() && AI->GetPawn() == Pawn.Get()
		? Cast<UNGPlayerAbilitySystemComponent>(AI->GetReadyAbilitySystem(Pawn.Get())) : nullptr;
	return ASC && ASC->GetAvatarGeneration() == AvatarGeneration;
}

void UNGAIGOAPComponent::InvalidatePlan(FName Reason)
{
	Plan = FNGAIPlan();
	LastReason = Reason;
	if (GetWorld()) NextRetryTime = GetWorld()->GetTimeSeconds() + (FMath::IsFinite(RetryDelay) ? FMath::Max(.1f, RetryDelay) : .5f);
}

bool UNGAIGOAPComponent::RefreshFacts()
{
	if (!HasCurrentLife()) { InvalidatePlan(TEXT("Gameplay life not ready")); return false; }
	auto* AI = Controller.Get();
	const auto* Target = AI->GetMemory()->GetTarget();
	const TWeakObjectPtr<AActor> TargetActor = Target ? Target->Actor : TWeakObjectPtr<AActor>();
	auto* Results = UNGAIQueryResultsComponent::Find(GetState());
	if (DecisionTarget != TargetActor)
	{
		DecisionTarget = TargetActor;
		Snapshot.Facts.Forget(ENGAIFact::Safe);
		InvalidatePlan(TEXT("Decision target changed"));
	}
	Snapshot.Facts.Set(ENGAIFact::HasTarget, Target != nullptr);
	Snapshot.Facts.Set(ENGAIFact::TargetVisible, Target && Target->bVisible);
	Snapshot.Facts.Set(ENGAIFact::AttackPerformed, false);
	auto* Combat = Pawn->FindComponentByClass<UNGCombatComponent>();
	const auto* Weapon = Combat ? Combat->GetWeaponInstance() : nullptr;
	const bool bEquipped = Weapon && Weapon->IsCurrent();
	Snapshot.Facts.Set(ENGAIFact::WeaponEquipped, bEquipped);
	bool bWeaponAvailable = bEquipped;
	const auto* PlayerState = Pawn->GetPlayerState();
	const auto* Inventory = PlayerState ? PlayerState->FindComponentByClass<UNGInventoryManagerComponent>() : nullptr;
	if (Inventory)
		for (auto* Item : Inventory->GetAllItems())
			if (Item && UNGCombatComponent::GetWeaponDefinition(Item->GetItemDef().GetDefaultObject())) { bWeaponAvailable = true; break; }
	Snapshot.Facts.Set(ENGAIFact::WeaponAvailable, bWeaponAvailable);
	Snapshot.Facts.Forget(ENGAIFact::MagazineLoaded);
	Snapshot.Facts.Forget(ENGAIFact::ReserveAvailable);
	if (bEquipped)
	{
		Snapshot.Facts.Set(ENGAIFact::MagazineLoaded, Combat->GetMagazineAmmo() > 0);
		Snapshot.Facts.Set(ENGAIFact::ReserveAvailable, Combat->GetReserveAmmo() > 0);
	}
	FNGAIQueryItem Position;
	const bool bHasPosition = Results && Results->GetQueryItem(PositionResultKey, 0, Position);
	Snapshot.Facts.Set(ENGAIFact::HasPosition, bHasPosition);
	Snapshot.Facts.Set(ENGAIFact::AtPosition, bHasPosition && FMath::IsFinite(PositionTolerance) && PositionTolerance > 0.f
		&& FVector::Dist(Pawn->GetActorLocation(), Position.Location) <= PositionTolerance);
	bool bClear = false;
	if (Target && Target->bVisible && Combat)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NGAIPlanFire), false, Pawn.Get());
		TArray<AActor*> Attached;
		Pawn->GetAttachedActors(Attached);
		Params.AddIgnoredActors(Attached);
		const auto ClearFrom = [&](const FVector& Origin)
		{
			FHitResult Hit;
			return !GetWorld()->LineTraceSingleByChannel(Hit, Origin, Target->LastSeenLocation, ECC_Visibility, Params)
				|| Hit.GetActor() == TargetActor.Get();
		};
		bClear = ClearFrom(Pawn->GetPawnViewLocation()) && ClearFrom(Combat->GetShotOrigin());
	}
	Snapshot.Facts.Set(ENGAIFact::LineOfFire, bClear);
	Snapshot.Inputs.Add(TEXT("TargetVisible"), Target && Target->bVisible ? 1.f : 0.f);
	Snapshot.Inputs.Remove(TEXT("TargetDistance"));
	if (Target && FMath::IsFinite(DistanceNormalization) && DistanceNormalization > 0.f)
		Snapshot.Inputs.Add(TEXT("TargetDistance"), FMath::Clamp(FVector::Dist(Pawn->GetActorLocation(), Target->GetKnownLocation()) / DistanceNormalization, 0.f, 1.f));
	const auto* ASC = AI->GetReadyAbilitySystem(Pawn.Get());
	const float MaxHealth = ASC->GetNumericAttribute(UNGHealthSet::GetMaxHealthAttribute());
	Snapshot.Inputs.Remove(TEXT("HealthFraction"));
	if (FMath::IsFinite(MaxHealth) && MaxHealth > 0.f)
		Snapshot.Inputs.Add(TEXT("HealthFraction"), FMath::Clamp(ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f));
	return true;
}
