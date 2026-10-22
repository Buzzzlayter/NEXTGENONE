// Exclusive publisher - Forward Gateway, LLC

#include "PreprocessorManager.h"

#include "GamepadAnalogCursor.h"
#include "Input/Events.h"

FPreprocessorManager::FPreprocessorManager(ULocalPlayer* Player)
	: GamepadCursor(MakeShared<FGamepadAnalogCursor>(Player))
{
}

FPreprocessorManager::~FPreprocessorManager() = default;

void FPreprocessorManager::Shutdown()
{
	InputDetector.OnInputTypeChanged.Unbind();
	GamepadCursor->SetGamepadCursorMode(EGamepadCursorMode::Disabled);
}

void FPreprocessorManager::SetInputMode(EInputMode Mode)
{
	InputMode = Mode;
}

void FPreprocessorManager::SetGamepadCursorMode(EGamepadCursorMode Mode)
{
	GamepadCursor->SetGamepadCursorMode(Mode);
}

bool FPreprocessorManager::ShouldHandleInput(const FInputEvent& Event) const
{
	return !GamepadCursor->IsGeneratingPointerInput()
		&& Event.GetUserIndex() == GamepadCursor->GetOwnerUserIndex()
		&& GamepadCursor->CanProcessInput();
}

void FPreprocessorManager::Tick(float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	if (InputMode != EInputMode::Game)
	{
		GamepadCursor->Tick(DeltaTime, SlateApp, Cursor);
	}
}

bool FPreprocessorManager::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event)
{
	if (!ShouldHandleInput(Event))
	{
		return false;
	}
	InputDetector.HandleKeyDownEvent(Event);
	return InputMode != EInputMode::Game && GamepadCursor->HandleKeyDownEvent(SlateApp, Event);
}

bool FPreprocessorManager::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event)
{
	return ShouldHandleInput(Event) && InputMode != EInputMode::Game
		&& GamepadCursor->HandleKeyUpEvent(SlateApp, Event);
}

bool FPreprocessorManager::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event)
{
	if (!ShouldHandleInput(Event))
	{
		return false;
	}
	InputDetector.HandleAnalogInputEvent(Event);
	return InputMode != EInputMode::Game && GamepadCursor->HandleAnalogInputEvent(SlateApp, Event);
}

bool FPreprocessorManager::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event)
{
	if (ShouldHandleInput(Event) && !Event.IsTouchEvent())
	{
		InputDetector.HandleMouseMoveEvent(Event);
	}
	return false;
}

bool FPreprocessorManager::DetectMouseInput(const FPointerEvent& Event)
{
	if (ShouldHandleInput(Event) && !Event.IsTouchEvent())
	{
		InputDetector.SetInputType(EInputType::KeyboardMouse);
	}
	return false;
}

bool FPreprocessorManager::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event)
{
	return DetectMouseInput(Event);
}

bool FPreprocessorManager::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event)
{
	return DetectMouseInput(Event);
}

bool FPreprocessorManager::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event)
{
	return DetectMouseInput(Event);
}

bool FPreprocessorManager::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event,
	const FPointerEvent* Gesture)
{
	return DetectMouseInput(Event);
}
