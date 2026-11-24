#pragma once

#include "HFSMState.h"
#include "Gameplay/AI/Planning/NGAIGOAPPlanner.h"
#include "NGAITacticalPlanningState.generated.h"

class APawn;
class UNGAIMemoryComponent;
class UNGAbilitySystemComponent;
class UNGInventoryItemDefinition;
struct FOnAttributeChangeData;
struct FNGTargetMemory;
enum class ENGAITacticalLane : uint8;

enum class ENGAITacticalGoal : uint8
{
	RegainContact,
	Pressure,
	ImproveAngle,
	Reload
};

UCLASS(Blueprintable, PrioritizeCategories = ("AI|Tactics"))
class NEXTGENONE_API UNGAITacticalPlanningState : public UHFSMState
{
	GENERATED_BODY()

public:
	FName CommitAction(const APawn& Pawn, ENGAITacticalLane Lane);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UNGAIPlanningProfile> PlanningProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics", meta = (ExposeOnSpawn = "true", ClampMin = "0"))
	float MinHoldDistance = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics", meta = (ExposeOnSpawn = "true", ClampMin = "0"))
	float MaxHoldDistance = 2400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics", meta = (ExposeOnSpawn = "true"))
	TSubclassOf<UNGInventoryItemDefinition> JetpackItemClass;

protected:
	virtual void Enter() override;
	virtual void Exit(FName Event) override;

private:
	void EvaluateGoal();
	void EvaluateBranches(const APawn& Pawn, const FNGTargetMemory* Target, bool bClearShot, double RangeEdge);
	void OnVitalChanged(const FOnAttributeChangeData& Data);
	void RefreshPlan(ENGAITacticalGoal NewGoal, const FNGAIDecisionSnapshot& Snapshot);
	void StopPlanning();

	TWeakObjectPtr<APawn> ExpectedPawn;
	TWeakObjectPtr<AActor> EvaluatedTarget;
	TWeakObjectPtr<UNGAIMemoryComponent> Memory;
	TWeakObjectPtr<UNGAbilitySystemComponent> ObservedASC;
	FDelegateHandle MemoryUpdatedHandle;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle ShieldChangedHandle;
	ENGAITacticalGoal Goal = ENGAITacticalGoal::RegainContact;
	FName PlannedGoal;
	FNGAIFacts PlannedFacts;
	FNGAIPlan Plan;
	int32 ConsecutivePressureChoices = 0;
	double NextOptionalFlankTime = 0.;
	double NextGroundTryTime = 0.;
	double NextJetpackEquipTime = 0.;
	double NextFlightTime = 0.;
	double NextWeaponEquipTime = 0.;
	double NextReloadTryTime = 0.;
	double NextFireTryTime = 0.;
	double LastDamageTime = -1.;
	FName MobilityChoice;
	FName WeaponChoice;
	FName ActiveMobilityAction;
	FName ActiveWeaponAction;
	bool bFlightWaitingForWeapon = false;
	bool bFlightActive = false;
};
