#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "NGAIPlanningProfile.generated.h"

UENUM(BlueprintType)
enum class ENGAIFact : uint8
{
	HasTarget, TargetVisible, WeaponAvailable, WeaponEquipped, MagazineLoaded,
	ReserveAvailable, HasPosition, AtPosition, LineOfFire, Safe, AttackPerformed,
	MagazineToppedUp, SearchPerformed,
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
