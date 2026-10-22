
#include "Input/PlayerInputSubsystem.h"
#include "Components/Widget.h"
#include "Context/HFSMContextLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputProcessors/PreprocessorManager.h"

UPlayerInputSubsystem* UPlayerInputSubsystem::GetInputSubsystem(const UObject* WCO)
{
	const ULocalPlayer* LocalPlayer = UHFSMContextLibrary::ResolveLocalPlayer(WCO);
	if (!LocalPlayer)
	{
		return nullptr;
	}

	return LocalPlayer->GetSubsystem<UPlayerInputSubsystem>();
}

void UPlayerInputSubsystem::SetInputMode(UObject* WCO, EInputMode Mode, bool bFlushInput)
{
	UPlayerInputSubsystem* PlayerInputSubsystem = GetInputSubsystem(WCO);
	if (!ensureMsgf(PlayerInputSubsystem, TEXT("Could not resolve the player input subsystem.")))
	{
		return;
	}
	PlayerInputSubsystem->SetPlayerInputMode(Mode, bFlushInput);
}

bool UPlayerInputSubsystem::SetPlayerInputMode(EInputMode Mode, bool bFlushInput)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!ensureMsgf(LocalPlayer, TEXT("Input subsystem has no local player.")))
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (!ensureMsgf(World, TEXT("Input subsystem has no world.")))
	{
		return false;
	}
	APlayerController* PlayerController = LocalPlayer->GetPlayerController(World);
	if (!ensureMsgf(PlayerController, TEXT("Input subsystem has no player controller.")))
	{
		return false;
	}
	if (!ensureMsgf(LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->GetGameViewportWidget().IsValid(),
		TEXT("Input subsystem has no game viewport widget.")))
	{
		return false;
	}

	switch (Mode)
	{
		case EInputMode::UI:
		{
			FInputModeUIOnly Input;
			Input.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
			PlayerController->SetInputMode(Input);
			break;
		}
		case EInputMode::GameAndUI:
		{
			FInputModeGameAndUI Input;
			Input.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
			Input.SetHideCursorDuringCapture(false);
			PlayerController->SetInputMode(Input);
			break;
		}
		case EInputMode::Game:
			PlayerController->SetInputMode(FInputModeGameOnly());
			break;
		default:
			ensureMsgf(false, TEXT("Input subsystem received an invalid input mode."));
			return false;
	}
	PlayerController->SetShowMouseCursor(Mode != EInputMode::Game);
	InputModeChanged(Mode);

	if (bFlushInput)
	{
		PlayerController->FlushPressedKeys();
	}
	return true;
}

void UPlayerInputSubsystem::SetWidgetToFocus(UWidget* WidgetToFocus)
{
	if (!ensureMsgf(WidgetToFocus, TEXT("Cannot focus a null widget.")))
	{
		return;
	}
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!ensureMsgf(LocalPlayer, TEXT("Input subsystem has no local player to focus a widget.")))
	{
		return;
	}
	if (!ensureMsgf(WidgetToFocus->GetOwningLocalPlayer() == LocalPlayer,
		TEXT("Cannot focus a widget owned by a different local player.")))
	{
		return;
	}
	FReply& SlateOperations = LocalPlayer->GetSlateOperations();
	SlateOperations.SetUserFocus(WidgetToFocus->TakeWidget());
}

EInputType UPlayerInputSubsystem::GetInputType() const
{
	return InputType;
}

EInputMode UPlayerInputSubsystem::GetInputMode() const
{
	return InputMode;
}

bool UPlayerInputSubsystem::IsGamepad() const
{
	return InputType == EInputType::Gamepad;
}

bool UPlayerInputSubsystem::IsKeyboardMouse() const
{
	return InputType == EInputType::KeyboardMouse;
}

void UPlayerInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	PreprocessorManager = MakeShared<FPreprocessorManager>(GetLocalPlayer());
	PreprocessorManager->SetInputMode(InputMode);
	PreprocessorManager->GetInputDetector().OnInputTypeChanged.BindUObject(this, &ThisClass::InputTypeChanged);
	if (!FSlateApplication::Get().RegisterInputPreProcessor(PreprocessorManager, 0))
	{
		PreprocessorManager->Shutdown();
		PreprocessorManager.Reset();
		UE_LOG(LogPlayerInputSubsystem, Error, TEXT("Failed to register UI input preprocessor."));
	}
}

void UPlayerInputSubsystem::Deinitialize()
{
	if (PreprocessorManager)
	{
		PreprocessorManager->Shutdown();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(PreprocessorManager);
		}
		PreprocessorManager.Reset();
	}
	Super::Deinitialize();
}

bool UPlayerInputSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

void UPlayerInputSubsystem::InputModeChanged(EInputMode Mode)
{
	const bool bChanged = InputMode != Mode;
	InputMode = Mode;
	if (PreprocessorManager)
	{
		PreprocessorManager->SetInputMode(Mode);
	}
	ApplyGamepadCursorMode();
	if (bChanged)
	{
		OnInputModeChanged.Broadcast(InputMode);
	}
}

void UPlayerInputSubsystem::InputTypeChanged(EInputType Type)
{
	if (InputType != Type)
	{
		InputType = Type;
		ApplyGamepadCursorMode();
		OnInputTypeChanged.Broadcast(InputType);
	}
}

void UPlayerInputSubsystem::SetGamepadCursorMode(EGamepadCursorMode Mode)
{
	GamepadInputMode = Mode;
	ApplyGamepadCursorMode();
}

void UPlayerInputSubsystem::ApplyGamepadCursorMode()
{
	if (PreprocessorManager)
	{
		PreprocessorManager->SetGamepadCursorMode(IsGamepad() && InputMode != EInputMode::Game
			? GamepadInputMode : EGamepadCursorMode::Disabled);
	}
}

void UPlayerInputSubsystem::ToggleCursorModes()
{
	SetGamepadCursorMode(GamepadInputMode == EGamepadCursorMode::Cursor
		? EGamepadCursorMode::Grid : EGamepadCursorMode::Cursor);
}
