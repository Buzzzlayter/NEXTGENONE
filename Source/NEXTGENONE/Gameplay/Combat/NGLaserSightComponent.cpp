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
	SetAttenuationRadius(10000.f);
	SetOuterConeAngle(0.2f);
	SetCastShadows(false);
	SetVisibility(false);
}

void UNGLaserSightComponent::SetTargetPoint(const FVector& Point)
{
	if (!IsUsingAbsoluteRotation()) SetAbsolute(false, true, true);
	SetWorldRotation((Point - GetComponentLocation()).Rotation());
}
