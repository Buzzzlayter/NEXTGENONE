// Exclusive publisher - Forward Gateway, LLC

#include "GamepadAnalogCursor.h"

#include "Engine/Console.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "Input/GameSettingsConfig.h"
#include "Layout/WidgetPath.h"
#include "Templates/UnrealTemplate.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

FGamepadAnalogCursor::FGamepadAnalogCursor(ULocalPlayer* Player)
	: LocalPlayer(Player)
{
}

int32 FGamepadAnalogCursor::GetOwnerUserIndex() const
{
	if (LocalPlayer.IsValid())
	{
		if (const TSharedPtr<FSlateUser> User = LocalPlayer->GetSlateUser())
		{
			return User->GetUserIndex();
		}
	}
	return INDEX_NONE;
}

UGameViewportClient* FGamepadAnalogCursor::GetViewportClient() const
{
	return LocalPlayer.IsValid() ? LocalPlayer->ViewportClient : nullptr;
}

bool FGamepadAnalogCursor::CanProcessInput() const
{
	const UGameViewportClient* Viewport = GetViewportClient();
	if (!Viewport || !Viewport->GetWorld() || !Viewport->GetWorld()->IsGameWorld()
		|| Viewport->IsSimulateInEditorViewport()
		|| !FSlateApplication::IsInitialized())
	{
		return false;
	}
	if (Viewport->ViewportConsole && Viewport->ViewportConsole->ConsoleActive())
	{
		return false;
	}
	const TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(GetOwnerUserIndex());
	const TSharedPtr<SWindow> Window = Viewport->GetWindow();
	const TSharedPtr<SViewport> ViewportWidget = Viewport->GetGameViewportWidget();
	return FSlateApplication::Get().IsActive() && User && Window
		&& ViewportWidget && User->IsWidgetInFocusPath(ViewportWidget)
		&& (User->HasFocus(Window).IsSet() || User->HasFocusedDescendants(Window.ToSharedRef()));
}

float FGamepadAnalogCursor::GetDPIScale() const
{
	const UGameViewportClient* Viewport = GetViewportClient();
	return Viewport && Viewport->Viewport
		? GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Viewport->Viewport->GetSizeXY()) : 1.f;
}

FVector2D FGamepadAnalogCursor::ClampToViewport(const FVector2D& Position) const
{
	if (const UGameViewportClient* Viewport = GetViewportClient())
	{
		if (const TSharedPtr<SViewport> Widget = Viewport->GetGameViewportWidget())
		{
			const FSlateRect Bounds = Widget->GetCachedGeometry().GetLayoutBoundingRect();
			return FVector2D(FMath::Clamp(Position.X, static_cast<double>(Bounds.Left), static_cast<double>(Bounds.Right)),
				FMath::Clamp(Position.Y, static_cast<double>(Bounds.Top), static_cast<double>(Bounds.Bottom)));
		}
	}
	return Position;
}

void FGamepadAnalogCursor::SetGamepadCursorMode(EGamepadCursorMode NewMode)
{
	if (GamepadCursorMode == NewMode)
	{
		return;
	}
	ReleasePointerButton();
	ClearAnalogValues();
	CurrentSpeed = FVector2D::ZeroVector;
	CurrentOffset = FVector2D::ZeroVector;
	LastCursorTarget.Reset();
	bSnapToFocusRequested = false;
	GamepadCursorMode = NewMode;
	if (NewMode == EGamepadCursorMode::Disabled)
	{
		RestoreCursor();
	}
}

void FGamepadAnalogCursor::RestoreCursor()
{
	if (!bCursorAppearanceApplied)
	{
		return;
	}
	if (UGameViewportClient* Viewport = GetViewportClient())
	{
		Viewport->SetSoftwareCursorWidget(EMouseCursor::Default, PreviousSoftwareCursor);
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetCursorRadius(PreviousCursorRadius);
		if (const TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(GetOwnerUserIndex()))
		{
			User->SetCursorVisibility(bPreviousCursorVisible);
		}
	}
	PreviousSoftwareCursor.Reset();
	CursorWidget.Reset();
	bCursorAppearanceApplied = false;
}

void FGamepadAnalogCursor::ReleasePointerButton()
{
	const TOptional<FKeyEvent> Event = PointerButtonEvent;
	PointerButtonEvent.Reset();
	if (Event.IsSet() && FSlateApplication::IsInitialized())
	{
		TGuardValue<bool> Guard(bGeneratingPointerInput, true);
		// Release while the old cursor mode is still active, including during subsystem teardown.
		FAnalogCursor::HandleKeyUpEvent(FSlateApplication::Get(), Event.GetValue());
	}
}

bool FGamepadAnalogCursor::IsRelevantInput(const FKeyEvent& Event) const
{
	return GamepadCursorMode == EGamepadCursorMode::Cursor
		&& Event.GetUserIndex() == GetOwnerUserIndex() && Event.GetKey().IsGamepadKey();
}

bool FGamepadAnalogCursor::IsRelevantInput(const FAnalogInputEvent& Event) const
{
	return GamepadCursorMode == EGamepadCursorMode::Cursor
		&& Event.GetUserIndex() == GetOwnerUserIndex() && Event.GetKey().IsGamepadKey();
}

bool FGamepadAnalogCursor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event)
{
	if (!IsRelevantInput(Event))
	{
		return false;
	}
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right
		|| Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down)
	{
		bSnapToFocusRequested = true;
	}
	if (Event.GetKey() == EKeys::Virtual_Gamepad_Accept.GetVirtualKey() && !Event.IsRepeat())
	{
		PointerButtonEvent = Event;
	}
	TGuardValue<bool> Guard(bGeneratingPointerInput, true);
	return FAnalogCursor::HandleKeyDownEvent(SlateApp, Event);
}

bool FGamepadAnalogCursor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event)
{
	if (!IsRelevantInput(Event))
	{
		return false;
	}
	if (Event.GetKey() == EKeys::Virtual_Gamepad_Accept.GetVirtualKey())
	{
		PointerButtonEvent.Reset();
	}
	TGuardValue<bool> Guard(bGeneratingPointerInput, true);
	return FAnalogCursor::HandleKeyUpEvent(SlateApp, Event);
}

bool FGamepadAnalogCursor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event)
{
	return IsRelevantInput(Event) && FAnalogCursor::HandleAnalogInputEvent(SlateApp, Event);
}

void FGamepadAnalogCursor::Tick(float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	if (GamepadCursorMode == EGamepadCursorMode::Disabled)
	{
		return;
	}
	if (!CanProcessInput())
	{
		ReleasePointerButton();
		ClearAnalogValues();
		CurrentSpeed = FVector2D::ZeroVector;
		CurrentOffset = FVector2D::ZeroVector;
		LastCursorTarget.Reset();
		RestoreCursor();
		return;
	}
	const TSharedPtr<FSlateUser> User = SlateApp.GetUser(GetOwnerUserIndex());
	if (!User)
	{
		return;
	}
	const UGameSettingsConfig* Settings = UGameSettingsConfig::Get();
	if (!bCursorAppearanceApplied)
	{
		UGameViewportClient* Viewport = GetViewportClient();
		PreviousSoftwareCursor = Viewport->GetSoftwareCursorWidget(EMouseCursor::Default);
		PreviousCursorRadius = SlateApp.GetCursorRadius();
		bPreviousCursorVisible = User->IsCursorVisible();
		bCursorAppearanceApplied = true;
		const FSoftClassPath& ClassPath = Settings->GetGamepadCursorWidgetClass();
		if (!ClassPath.IsNull())
		{
			if (UClass* WidgetClass = ClassPath.TryLoadClass<UGamepadCursorWidget>())
			{
				APlayerController* Controller = LocalPlayer->GetPlayerController(LocalPlayer->GetWorld());
				CursorWidget.Reset(CreateWidget<UGamepadCursorWidget>(Controller, WidgetClass));
				if (CursorWidget.IsValid())
				{
					CursorWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
					Viewport->SetSoftwareCursorWidget(EMouseCursor::Default, CursorWidget.Get());
				}
			}
		}
	}
	User->SetCursorVisibility(GamepadCursorMode == EGamepadCursorMode::Cursor);
	SlateApp.SetCursorRadius(GamepadCursorMode == EGamepadCursorMode::Cursor
		? Settings->GetGamepadCursorRadius() * GetDPIScale() : 0.f);

	TGuardValue<bool> Guard(bGeneratingPointerInput, true);
	if (GamepadCursorMode == EGamepadCursorMode::Grid || bSnapToFocusRequested)
	{
		bSnapToFocusRequested = false;
		if (const TSharedPtr<SWidget> FocusedWidget = User->GetFocusedWidget())
		{
			const FGeometry& Geometry = FocusedWidget->GetTickSpaceGeometry();
			if (FocusedWidget->IsEnabled() && FocusedWidget->IsInteractable()
				&& !Geometry.GetLocalSize().IsNearlyZero())
			{
				const FVector2D Target = Geometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5));
				UpdateCursorPosition(SlateApp, User.ToSharedRef(), ClampToViewport(Target));
			}
		}
		return;
	}
	UpdateHoveredTarget(SlateApp, User.ToSharedRef());
	UpdateCursorPosition(SlateApp, User.ToSharedRef(),
		ClampToViewport(CalculateTickedCursorPosition(DeltaTime, SlateApp, User)));
	if (GamepadCursorMode == EGamepadCursorMode::Cursor)
	{
		Scroll(DeltaTime, SlateApp, User.ToSharedRef());
	}
}

FVector2D FGamepadAnalogCursor::CalculateTickedCursorPosition(float DeltaTime, FSlateApplication& SlateApp,
	TSharedPtr<FSlateUser> SlateUser)
{
	const UGameSettingsConfig* Settings = UGameSettingsConfig::Get();
	const float DPIScale = GetDPIScale();
	const float DeadZoneSize = FMath::Clamp(Settings->GetGamepadCursorDeadZone(), 0.f, 0.99f);
	const FVector2D RawStick = GetAnalogValues(EAnalogStick::Left);
	const float Magnitude = FMath::Min(static_cast<float>(RawStick.Size()), 1.f);
	const FVector2D Stick = RawStick.GetSafeNormal()
		* FMath::Max(Magnitude - DeadZoneSize, 0.f) / (1.f - DeadZoneSize);
	const float Speed = (LastCursorTarget.IsValid() ? Settings->GetMaxGamepadCursorSpeedWhenHovered()
		: Settings->GetMaxGamepadCursorSpeed()) * DPIScale;
	const float AccelerationScale = Settings->GetGamepadCursorAccelerationMultiplier() * DPIScale;
	FVector2D AccelerationValue = FVector2D::ZeroVector;
	switch (Settings->GetGamepadAccelerationType())
	{
	case EGamepadAccelerationType::Cubic:
		AccelerationValue = Stick * Stick * Stick * AccelerationScale;
		break;
	case EGamepadAccelerationType::Linear:
		AccelerationValue = FVector2D(FMath::Sign(Stick.X), FMath::Sign(Stick.Y)) * AccelerationScale;
		break;
	case EGamepadAccelerationType::Curve:
		if (const FRichCurve* Curve = Settings->GetGamepadCursorAccelerationCurve())
		{
			AccelerationValue = Stick.GetSafeNormal() * Curve->Eval(Stick.Size()) * AccelerationScale;
		}
		break;
	}
	CurrentSpeed += AccelerationValue * DeltaTime;
	CurrentSpeed.X = FMath::Clamp(CurrentSpeed.X, FMath::Min(Stick.X * Speed, 0.0), FMath::Max(Stick.X * Speed, 0.0));
	CurrentSpeed.Y = FMath::Clamp(CurrentSpeed.Y, FMath::Min(Stick.Y * Speed, 0.0), FMath::Max(Stick.Y * Speed, 0.0));
	CurrentOffset += CurrentSpeed * DeltaTime;
	const FVector2D Position = SlateUser->GetCursorPosition() + CurrentOffset;
	CurrentOffset.X = FGenericPlatformMath::Frac(Position.X);
	CurrentOffset.Y = FGenericPlatformMath::Frac(Position.Y);
	return Position;
}

void FGamepadAnalogCursor::UpdateHoveredTarget(FSlateApplication& SlateApp, const TSharedRef<FSlateUser>& User)
{
	const TSharedPtr<SWidget> PreviousTarget = LastCursorTarget.Pin();
	LastCursorTarget.Reset();
	const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(User->GetCursorPosition(),
		SlateApp.GetInteractiveTopLevelWindows(), false, GetOwnerUserIndex());
	for (int32 Index = Path.Widgets.Num() - 1; Index >= 0; --Index)
	{
		const TSharedRef<SWidget> Widget = Path.Widgets[Index].Widget;
		if (Widget->IsEnabled() && Widget->IsInteractable() && Widget->SupportsKeyboardFocus())
		{
			LastCursorTarget = Widget;
			if (Widget != User->GetFocusedWidget())
			{
				SlateApp.SetUserFocus(GetOwnerUserIndex(), Widget, EFocusCause::Mouse);
			}
			break;
		}
	}
	if (CursorWidget.IsValid() && PreviousTarget != LastCursorTarget.Pin())
	{
		CursorWidget->OnFocusFound(LastCursorTarget.IsValid());
	}
}

void FGamepadAnalogCursor::Scroll(float DeltaTime, FSlateApplication& SlateApp, const TSharedRef<FSlateUser>& User)
{
	const UGameSettingsConfig* Settings = UGameSettingsConfig::Get();
	const float Value = static_cast<float>(GetAnalogValues(EAnalogStick::Right).Y);
	const float ScrollDeadZone = FMath::Clamp(Settings->GetGamepadScrollDeadZone(), 0.f, 0.99f);
	if (FMath::Abs(Value) <= ScrollDeadZone)
	{
		return;
	}
	const float Amount = -(FMath::Abs(Value) - ScrollDeadZone) / (1.f - ScrollDeadZone)
		* FMath::Sign(Value) * Settings->GetGamepadScrollMultiplier() * DeltaTime * 60.f;
	const FPointerEvent Event(GetOwnerUserIndex(), FSlateApplication::CursorPointerIndex,
		User->GetCursorPosition(), User->GetCursorPosition(), TSet<FKey>(), EKeys::MouseWheelAxis,
		Amount, FModifierKeysState());
	SlateApp.ProcessMouseWheelOrGestureEvent(Event, nullptr);
}
