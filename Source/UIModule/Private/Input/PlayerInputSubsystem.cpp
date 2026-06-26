
#include "Input/PlayerInputSubsystem.h"
#include "Components/Widget.h"

#include "Widgets/SViewport.h"

UPlayerInputSubsystem* UPlayerInputSubsystem::GetInputSubsystem(const UObject* WCO)
{
	const auto World = WCO->GetWorld();
	const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
	return LocalPlayer->GetSubsystem<UPlayerInputSubsystem>();
}

void UPlayerInputSubsystem::SetInputMode(UObject* WCO, EInputMode Mode, bool bFlushInput)
{
	const auto World = WCO->GetWorld();
	ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
	APlayerController* PlayerController = LocalPlayer->GetPlayerController(World);
	UPlayerInputSubsystem* PlayerInputSubsystem = GetInputSubsystem(WCO);
	PlayerInputSubsystem->InputModeChanged(Mode);

	FReply& SlateOperations = LocalPlayer->GetSlateOperations();
	UGameViewportClient* GameViewport = World->GetGameViewport();
	
	switch (Mode)
	{
		case EInputMode::UI:
			SlateOperations.ReleaseMouseCapture();
			GameViewport->SetMouseLockMode(EMouseLockMode::LockInFullscreen);
			GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);

			PlayerController->SetShowMouseCursor(true);
			break;

		case EInputMode::GameAndUI:
			SlateOperations.ReleaseMouseCapture();
			GameViewport->SetMouseLockMode(EMouseLockMode::LockInFullscreen);
			GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);

			PlayerController->SetShowMouseCursor(true);
			break;

		case EInputMode::Game:
			const TSharedPtr<SViewport> ViewportWidget = GameViewport->GetGameViewportWidget();
			const TSharedRef<SViewport> ViewportWidgetRef = ViewportWidget.ToSharedRef();
			SlateOperations.UseHighPrecisionMouseMovement(ViewportWidgetRef);
			SlateOperations.SetUserFocus(ViewportWidgetRef);
			SlateOperations.LockMouseToWidget(ViewportWidgetRef);
			GameViewport->SetMouseLockMode(EMouseLockMode::LockOnCapture);
			GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently);

			PlayerController->SetShowMouseCursor(false);
			break;
	}

	if (bFlushInput)
	{
		PlayerController->FlushPressedKeys();
	}
}

void UPlayerInputSubsystem::SetWidgetToFocus(UWidget* WidgetToFocus)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
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
	Collection.InitializeDependency<UPlayerInputSubsystem>();
}

void UPlayerInputSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool UPlayerInputSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
/*#if WITH_EDITOR
	auto World = Outer ? Outer->GetWorld() : nullptr;
	if (World && World->WorldType == EWorldType::PIE && GEditor->IsSimulateInEditorInProgress())
	{
		return false;
	}
#endif*/
	return Super::ShouldCreateSubsystem(Outer);
}

void UPlayerInputSubsystem::InputModeChanged(EInputMode Mode)
{
	InputMode = Mode;
	OnInputModeChanged.Broadcast(InputMode);

	//UE_LOG(LogPlayerInputSubsystem, Log, TEXT("Input mode changed: %s"), *UEnum::GetValueAsString(Mode));

	if (InputMode == EInputMode::Game)
	{
		SetGamepadCursorMode(EGamepadCursorMode::Disabled);
	}
}

void UPlayerInputSubsystem::InputTypeChanged(EInputType Type)
{
	InputType = Type;
	OnInputTypeChanged.Broadcast(InputType);
	//UE_LOG(LogPlayerInputSubsystem, Log, TEXT("Input type changed: %s"), *UEnum::GetValueAsString(Type));

	SetGamepadCursorMode(GamepadInputMode);
}

bool UPlayerInputSubsystem::ContainsPreProcessorManager() const
{
	if (PreprocessorManager.IsValid())
	{
		// TODO:: REFACTOR
		/*const int32 FoundIndex =
			FSlateApplication::Get().FindInputPreProcessor(PreprocessorManager), EInputPreProcessorType::Game);
		return (FoundIndex > INDEX_NONE);*/
	}
	return false;
}

void UPlayerInputSubsystem::SetGamepadCursorMode(EGamepadCursorMode Mode)
{
	GamepadInputMode = Mode;
}

void UPlayerInputSubsystem::ToggleCursorModes()
{
	// TODO:: REFACTOR
	/*static const UEnum* ModeEnum = StaticEnum<EGamepadCursorMode>();
	int32 NextSnappingMode = static_cast<int32>(GamepadInputMode) + 1;

	if (ModeEnum->GetMaxEnumValue() <= NextSnappingMode)
	{
		NextSnappingMode = 1;
	}
	SetGamepadCursorMode(static_cast<EGamepadCursorMode>(NextSnappingMode));*/
}
