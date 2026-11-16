#pragma once

#include "CoreMinimal.h"
#include "NGJetpackMovementSettings.generated.h"

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
