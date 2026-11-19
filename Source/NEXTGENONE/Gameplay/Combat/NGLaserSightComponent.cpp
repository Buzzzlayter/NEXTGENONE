#include "Gameplay/Combat/NGLaserSightComponent.h"

UNGLaserSightComponent::UNGLaserSightComponent()
{
	SetAbsolute(false, false, true);
	SetIntensityUnits(ELightUnits::Lumens);
	SetIntensity(2000.f);
	SetIndirectLightingIntensity(0.f);
	SetVolumetricScatteringIntensity(0.f);
	SetAffectGlobalIllumination(false);
	SetSpecularScale(0.f);
	SetAffectReflection(false);
	SetLightColor(FLinearColor::Red);
	SetAttenuationRadius(MaxProjectionDistance);
	SetOuterConeAngle(0.2f);
	SetCastShadows(false);
	SetVisibility(false);
}

void UNGLaserSightComponent::SetTargetPoint(const FVector& Point)
{
	const FVector ToTarget = Point - GetComponentLocation();
	SetAttenuationRadius(FMath::Min(MaxProjectionDistance, static_cast<float>(ToTarget.Size()) + 25.f));
	if (!IsUsingAbsoluteRotation()) SetAbsolute(false, true, true);
	SetWorldRotation(ToTarget.Rotation());
}
