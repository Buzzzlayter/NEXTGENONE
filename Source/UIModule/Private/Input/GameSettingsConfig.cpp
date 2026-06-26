

#include "Input/GameSettingsConfig.h"

UGameSettingsConfig::UGameSettingsConfig()
{
	GamepadCursorAccelerationCurve.EditorCurveData.AddKey(0, 0);
	GamepadCursorAccelerationCurve.EditorCurveData.AddKey(1, 1);
}

const UGameSettingsConfig* UGameSettingsConfig::Get()
{
	return GetDefault<UGameSettingsConfig>();
}

const FRichCurve* UGameSettingsConfig::GetGamepadCursorAccelerationCurve() const
{
	return GamepadCursorAccelerationCurve.GetRichCurveConst();
}

float UGameSettingsConfig::GetMaxGamepadCursorSpeed() const
{
	return MaxGamepadCursorSpeed;
}

float UGameSettingsConfig::GetMaxGamepadCursorSpeedWhenHovered() const
{
	return MaxGamepadCursorSpeedWhenHovered;
}

float UGameSettingsConfig::GetGamepadCursorAccelerationMultiplier() const
{
	return GamepadCursorAccelerationMultiplier;
}

EGamepadAccelerationType UGameSettingsConfig::GetGamepadAccelerationType() const
{
	return GamepadAccelerationType;
}

float UGameSettingsConfig::GetGamepadCursorDeadZone() const
{
	return GamepadCursorDeadZone;
}

float UGameSettingsConfig::GetGamepadCursorSize() const
{
	return FMath::Max<float>(GamepadCursorSize, 1.0f);
}

const FSoftClassPath& UGameSettingsConfig::GetGamepadCursorWidgetClass() const
{
	return GamepadCursorWidget;
}

float UGameSettingsConfig::GetGamepadCursorReleaseThreshold() const
{
	return GamepadCursorReleaseThreshold;
}

FVector2D UGameSettingsConfig::GetGamepadCursorSizeVector() const
{
	return FVector2D(GetGamepadCursorSize(), GetGamepadCursorSize());
}

float UGameSettingsConfig::GetGamepadCursorRadius() const
{
	return GetGamepadCursorSize() / 2.0f;
}

float UGameSettingsConfig::GetGamepadScrollDeadZone() const
{
	return GamepadScrollDeadZone;
}

float UGameSettingsConfig::GetGamepadScrollMultiplier() const
{
	return GamepadScrollMultiplier;
}

float UGameSettingsConfig::GetMouseMoveThreshold() const
{
	return MouseMoveThreshold;
}

float UGameSettingsConfig::GetGamepadAxisThreshold() const
{
	return GamepadAxisThreshold;
}

FName UGameSettingsConfig::GetCategoryName() const
{
	return TEXT("Game");
}
