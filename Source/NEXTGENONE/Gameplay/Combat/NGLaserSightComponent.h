#pragma once

#include "Components/SpotLightComponent.h"
#include "NGLaserSightComponent.generated.h"

UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGLaserSightComponent : public USpotLightComponent
{
	GENERATED_BODY()
public:
	static constexpr float MaxProjectionDistance = 10000.f;
	UNGLaserSightComponent();
	void SetTargetPoint(const FVector& Point);
};
