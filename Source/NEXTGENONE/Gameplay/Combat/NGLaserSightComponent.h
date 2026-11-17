#pragma once

#include "Components/SpotLightComponent.h"
#include "NGLaserSightComponent.generated.h"

UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGLaserSightComponent : public USpotLightComponent
{
	GENERATED_BODY()
public:
	UNGLaserSightComponent();
	void SetTargetPoint(const FVector& Point);
};
