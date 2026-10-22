// Exclusive publisher - Forward Gateway, LLC

#pragma once

#include "CoreMinimal.h"
#include "Input/InputData.h"

struct FKeyEvent;
struct FAnalogInputEvent;
struct FPointerEvent;

class FInputTypeDetector
{
public:
	DECLARE_DELEGATE_OneParam(FOnInputTypeDetected, EInputType);
	FOnInputTypeDetected OnInputTypeChanged;

	void HandleKeyDownEvent(const FKeyEvent& Event);
	void HandleAnalogInputEvent(const FAnalogInputEvent& Event);
	void HandleMouseMoveEvent(const FPointerEvent& Event);
	void SetInputType(EInputType Type);

private:
	EInputType LastInputType = EInputType::KeyboardMouse;
};
