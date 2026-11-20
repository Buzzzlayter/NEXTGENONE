#include "Gameplay/Combat/NGLaserSightComponent.h"

UNGLaserSightComponent::UNGLaserSightComponent()
{
	SetAbsolute(false, false, true);
	SetUseInverseSquaredFalloff(true);
	SetIntensityUnits(ELightUnits::Lumens);
	SetIntensity(8500.f);
	SetInverseExposureBlend(0.65f);
	SetIndirectLightingIntensity(0.f);
	SetVolumetricScatteringIntensity(0.f);
	SetAffectGlobalIllumination(false);
	SetSpecularScale(0.f);
	SetAffectReflection(false);
	SetLightColor(FLinearColor(1.f, 0.025f, 0.015f));
	SetAttenuationRadius(MaxProjectionDistance + 25.f);
	SetInnerConeAngle(0.12f);
	SetOuterConeAngle(0.32f);
	SetCastShadows(false);
	SetVisibility(false);
}

void UNGLaserSightComponent::SetTargetPoint(const FVector& Point)
{
	const FVector ToTarget = Point - GetComponentLocation();
	SetAttenuationRadius(FMath::Min(MaxProjectionDistance, static_cast<float>(ToTarget.Size())) + 25.f);
	if (!IsUsingAbsoluteRotation()) SetAbsolute(false, true, true);
	SetWorldRotation(ToTarget.Rotation());
}
