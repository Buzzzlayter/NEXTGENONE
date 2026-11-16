#include "Gameplay/Character/NGJetpackMovementSettings.h"

bool FNGJetpackMovementSettings::IsValid() const
{
	return FMath::IsFinite(Acceleration) && Acceleration > 0.f && FMath::IsFinite(MaxRiseSpeed) && MaxRiseSpeed > 0.f
		&& FMath::IsFinite(AirControl) && AirControl >= 0.f && AirControl <= 1.f
		&& FMath::IsFinite(ThrustRampTime) && ThrustRampTime >= 0.f
		&& FMath::IsFinite(HorizontalSpeed) && HorizontalSpeed > 0.f
		&& FMath::IsFinite(HorizontalAcceleration) && HorizontalAcceleration >= 0.f
		&& FMath::IsFinite(HorizontalBraking) && HorizontalBraking >= 0.f;
}
