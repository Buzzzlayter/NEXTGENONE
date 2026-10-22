// Exclusive publisher - Forward Gateway, LLC

#pragma once

#include "Framework/Application/AnalogCursor.h"
#include "Input/Events.h"
#include "Input/InputData.h"
#include "Input/GamepadCursorWidget.h"
#include "UObject/StrongObjectPtr.h"

class ULocalPlayer;
class UGameViewportClient;

class FGamepadAnalogCursor : public FAnalogCursor
{
public:
	explicit FGamepadAnalogCursor(ULocalPlayer* Player);
	virtual int32 GetOwnerUserIndex() const override;
	virtual void Tick(float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("GamepadCursor"); }

	void SetGamepadCursorMode(EGamepadCursorMode NewMode);
	bool CanProcessInput() const;
	bool IsGeneratingPointerInput() const { return bGeneratingPointerInput; }

protected:
	virtual bool IsRelevantInput(const FKeyEvent& Event) const override;
	virtual bool IsRelevantInput(const FAnalogInputEvent& Event) const override;
	virtual FVector2D CalculateTickedCursorPosition(float DeltaTime, FSlateApplication& SlateApp,
		TSharedPtr<FSlateUser> SlateUser) override;

private:
	UGameViewportClient* GetViewportClient() const;
	float GetDPIScale() const;
	FVector2D ClampToViewport(const FVector2D& Position) const;
	void UpdateHoveredTarget(FSlateApplication& SlateApp, const TSharedRef<FSlateUser>& User);
	void Scroll(float DeltaTime, FSlateApplication& SlateApp, const TSharedRef<FSlateUser>& User);
	void ReleasePointerButton();
	void RestoreCursor();

	TWeakObjectPtr<ULocalPlayer> LocalPlayer;
	TStrongObjectPtr<UGamepadCursorWidget> CursorWidget;
	TWeakPtr<SWidget> LastCursorTarget;
	TSharedPtr<SWidget> PreviousSoftwareCursor;
	TOptional<FKeyEvent> PointerButtonEvent;
	EGamepadCursorMode GamepadCursorMode = EGamepadCursorMode::Disabled;
	float PreviousCursorRadius = 0.f;
	bool bGeneratingPointerInput = false;
	bool bCursorAppearanceApplied = false;
	bool bPreviousCursorVisible = true;
	bool bSnapToFocusRequested = false;
};
