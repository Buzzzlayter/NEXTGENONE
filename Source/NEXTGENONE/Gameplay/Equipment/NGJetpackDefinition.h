#pragma once

#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "NGJetpackDefinition.generated.h"

USTRUCT()
struct NEXTGENONE_API FNGJetpackMovementSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float Acceleration = 2200.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float MaxRiseSpeed = 700.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="1")) float AirControl = .5f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float ThrustRampTime = .2f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float HorizontalSpeed = 800.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float HorizontalAcceleration = 1800.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float HorizontalBraking = 450.f;
	bool IsValid() const;
};

UCLASS(BlueprintType)
class NEXTGENONE_API UNGJetpackDefinition : public UNGEquipmentDefinition
{
	GENERATED_BODY()
public:
	UNGJetpackDefinition();
	UPROPERTY(EditDefaultsOnly, Category="Jetpack") FNGJetpackMovementSettings Movement;
	UPROPERTY(EditDefaultsOnly, Category="Jetpack", meta=(ClampMin="0.1", ClampMax="30")) float MaxBurnDuration = 4.f;
	virtual bool ValidateDefinition(FString& Error) const override;
};

/** Wearing and active thrust are independent tags/effects. */
UCLASS()
class NEXTGENONE_API UNGJetpackWearingEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGJetpackWearingEffect();
};
