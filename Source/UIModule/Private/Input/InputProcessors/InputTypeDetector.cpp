// Exclusive publisher - Forward Gateway, LLC

#include "InputTypeDetector.h"

#include "Input/Events.h"
#include "Input/GameSettingsConfig.h"

void FInputTypeDetector::HandleKeyDownEvent(const FKeyEvent& Event)
{
	SetInputType(Event.GetKey().IsGamepadKey() ? EInputType::Gamepad : EInputType::KeyboardMouse);
}

void FInputTypeDetector::HandleAnalogInputEvent(const FAnalogInputEvent& Event)
{
	if (Event.GetKey().IsGamepadKey()
		&& FMath::Abs(Event.GetAnalogValue()) > UGameSettingsConfig::Get()->GetGamepadAxisThreshold())
	{
		SetInputType(EInputType::Gamepad);
	}
}

void FInputTypeDetector::HandleMouseMoveEvent(const FPointerEvent& Event)
{
	const FVector2D Delta = Event.GetCursorDelta();
	const float Threshold = UGameSettingsConfig::Get()->GetMouseMoveThreshold();
	if (FMath::Abs(Delta.X) > Threshold || FMath::Abs(Delta.Y) > Threshold)
	{
		SetInputType(EInputType::KeyboardMouse);
	}
}

void FInputTypeDetector::SetInputType(EInputType Type)
{
	if (Type != LastInputType)
	{
		LastInputType = Type;
		OnInputTypeChanged.ExecuteIfBound(Type);
	}
}
