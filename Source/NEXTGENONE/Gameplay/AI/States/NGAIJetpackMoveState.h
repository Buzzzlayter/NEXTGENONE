#pragma once

#include "Gameplay/AI/States/NGAIActionState.h"
#include "Gameplay/AI/States/NGAIAbilityInput.h"
#include "NGAIJetpackMoveState.generated.h"

class UNGCharacterMovementComponent;
class UEnvQuery;

UCLASS(Blueprintable, DisplayName="AI Jetpack Move State", PrioritizeCategories=("Flight"))
class NEXTGENONE_API UNGAIJetpackMoveState : public UNGAIActionState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ExposeOnSpawn="true"))
	FVector Destination = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ExposeOnSpawn="true", ClampMin="0.1", Units="s"))
	float ThrustDuration = .6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ExposeOnSpawn="true", ClampMin="0.5", ClampMax="2.0"))
	float ThrustScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ExposeOnSpawn="true", ClampMin="1", Units="cm"))
	float AcceptanceRadius = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ExposeOnSpawn="true", ClampMin="0", ClampMax="1"))
	float SteeringAirControl = .85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ExposeOnSpawn="true"))
	TObjectPtr<UEnvQuery> LandingQuery;

protected:
	virtual void StartAction() override;
	virtual void Tick(float DeltaTime) override;
	virtual void ReleaseActionResources() override;

private:
	bool ProjectLandingPoint(const FVector& Point, FVector& ProjectedPoint) const;
	bool FindNearbyLandingPoint(FVector& ProjectedPoint) const;
	void RequestLandingQuery();
	FNGAIAbilityInput JetpackInput;
	TWeakObjectPtr<UNGCharacterMovementComponent> FlightMovement;
	FVector LandingPoint = FVector::ZeroVector;
	float LandingUpdateRemaining = 0.f;
	int32 LandingQueryID = INDEX_NONE;
	bool bWasAirborne = false;
	bool bBrakeForEdge = false;
};
