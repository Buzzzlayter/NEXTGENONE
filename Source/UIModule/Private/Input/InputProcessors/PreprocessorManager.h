// Exclusive publisher - Forward Gateway, LLC

#pragma once

#include "Framework/Application/IInputProcessor.h"
#include "Input/InputData.h"
#include "InputTypeDetector.h"

class FGamepadAnalogCursor;
class ULocalPlayer;
struct FInputEvent;

class FPreprocessorManager : public IInputProcessor
{
public:
	explicit FPreprocessorManager(ULocalPlayer* Player);
	virtual ~FPreprocessorManager() override;

	FInputTypeDetector& GetInputDetector() { return InputDetector; }
	void SetInputMode(EInputMode Mode);
	void SetGamepadCursorMode(EGamepadCursorMode Mode);
	void Shutdown();

	virtual void Tick(float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event,
		const FPointerEvent* Gesture) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("UIInputPreprocessor"); }

private:
	bool ShouldHandleInput(const FInputEvent& Event) const;
	bool DetectMouseInput(const FPointerEvent& Event);
	TSharedPtr<FGamepadAnalogCursor> GamepadCursor;
	FInputTypeDetector InputDetector;
	EInputMode InputMode = EInputMode::Game;
};
