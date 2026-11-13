#pragma once

#include "HFSMStateComponent.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "NGAIGOAPComponent.generated.h"

class ANGAIController;
class APawn;
class AActor;

UENUM(BlueprintType)
enum class ENGAIFact : uint8
{
	HasTarget, TargetVisible, WeaponAvailable, WeaponEquipped, MagazineLoaded,
	ReserveAvailable, HasPosition, AtPosition, LineOfFire, Safe, AttackPerformed,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ENGAIPlanStatus : uint8 { Succeeded, NoPlan, BudgetExceeded, InvalidConfiguration };

USTRUCT()
struct FNGAIFactValue
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) ENGAIFact Fact = ENGAIFact::HasTarget;
	UPROPERTY(EditAnywhere) bool Value = true;
};

/** Curves read normalized inputs from a fixed decision snapshot. */
USTRUCT()
struct FNGAIScoreTerm
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Input;
	UPROPERTY(EditAnywhere) FRuntimeFloatCurve Curve;
};

USTRUCT()
struct FNGAIPlannedAction
{
	GENERATED_BODY()
	/** Returned to the behavior graph; this description never executes gameplay. */
	UPROPERTY(EditAnywhere) FName Id;
	UPROPERTY(EditAnywhere) TArray<FNGAIFactValue> Preconditions;
	UPROPERTY(EditAnywhere) TArray<FNGAIFactValue> Effects;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float BaseCost = 1.f;
	/** Nonnegative curve outputs are added to BaseCost. Missing inputs disable the action. */
	UPROPERTY(EditAnywhere) TArray<FNGAIScoreTerm> CostTerms;
};

USTRUCT()
struct FNGAIPlanningGoal
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Id;
	UPROPERTY(EditAnywhere) TArray<FNGAIFactValue> Eligibility;
	UPROPERTY(EditAnywhere) TArray<FNGAIFactValue> DesiredState;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float Priority = 1.f;
	/** Outputs in [0,1] multiply Priority; no terms means constant Priority. */
	UPROPERTY(EditAnywhere) TArray<FNGAIScoreTerm> UtilityTerms;
};

UCLASS()
class NEXTGENONE_API UNGAIPlanningProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Planning") TArray<FNGAIPlanningGoal> Goals;
	UPROPERTY(EditAnywhere, Category="Planning") TArray<FNGAIPlannedAction> Actions;
	UPROPERTY(EditAnywhere, Category="Planning", meta=(ClampMin="1", ClampMax="64")) int32 MaxPlanDepth = 12;
	UPROPERTY(EditAnywhere, Category="Planning", meta=(ClampMin="1", ClampMax="4096")) int32 MaxSearchNodes = 512;
};

/** Unknown is separate from false. Facts are a projection of gameplay, never its owner. */
struct FNGAIFacts
{
	uint64 Known = 0;
	uint64 Values = 0;
	void Set(ENGAIFact Fact, bool Value);
	void Forget(ENGAIFact Fact);
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


/** Parked GOAP experiment. No task binding or execution. */
UCLASS(EditInlineNew, Blueprintable, meta=(DisplayName="AI GOAP Planner"))
class NEXTGENONE_API UNGAIGOAPComponent : public UHFSMStateComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="AI|Planning") TObjectPtr<UNGAIPlanningProfile> Profile;
	UPROPERTY(EditAnywhere, Category="AI|Planning", meta=(ClampMin="0")) float SwitchMargin = 0.1f;
	UPROPERTY(EditAnywhere, Category="AI|Planning", meta=(ClampMin="0.1")) float RetryDelay = 0.5f;
	UPROPERTY(EditAnywhere, Category="AI|Facts") FName PositionResultKey = TEXT("MovePosition");
	UPROPERTY(EditAnywhere, Category="AI|Facts", meta=(ClampMin="1")) float PositionTolerance = 150.f;
	UPROPERTY(EditAnywhere, Category="AI|Facts", meta=(ClampMin="1")) float DistanceNormalization = 3000.f;
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
	void SetFact(ENGAIFact Fact, bool Value);
	void SetInput(FName Input, float Value);
	/** Experimental, explicit evaluation only. Does not execute actions or transition FSMs. */
	bool Replan();
	UFUNCTION(BlueprintPure, Category="AI|Planning") FName GetLastReason() const { return LastReason; }
	UFUNCTION(BlueprintPure, Category="AI|Planning") FName GetCurrentGoal() const { return CurrentGoal; }
	UFUNCTION(BlueprintPure, Category="AI|Planning") TArray<FName> GetPlanActions() const { return Plan.Actions; }
	UFUNCTION(BlueprintPure, Category="AI|Planning") ENGAIPlanStatus GetPlanStatus() const { return Plan.Status; }
	const FNGAIPlan& GetPlan() const { return Plan; }
private:
	void ClearSnapshot();
	bool RefreshFacts();
	FNGAIDecisionSnapshot Snapshot;
	FNGAIPlan Plan;
	FName CurrentGoal;
	TWeakObjectPtr<ANGAIController> Controller;
	TWeakObjectPtr<APawn> Pawn;
	uint32 AvatarGeneration = 0;
	TWeakObjectPtr<AActor> DecisionTarget;
	double NextRetryTime = 0.;
	FName LastReason;
	bool HasCurrentLife() const;
	void InvalidatePlan(FName Reason);
};
