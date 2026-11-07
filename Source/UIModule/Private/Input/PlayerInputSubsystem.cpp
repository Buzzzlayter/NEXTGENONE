
#include "Input/PlayerInputSubsystem.h"
#include "Components/Widget.h"
#include "HFSMLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputProcessors/PreprocessorManager.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

UPlayerInputSubsystem* UPlayerInputSubsystem::GetInputSubsystem(const UObject* WCO)
{
	const ULocalPlayer* LocalPlayer = UHFSMLibrary::ResolveLocalPlayer(WCO);
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
	if (!IsValid(LocalPlayer)) return false;
	
	UWorld* World = GetWorld();
	// LocalPlayerSubsystem outlives the world during travel/PIE shutdown.
	if (!IsValid(World) || World->bIsTearingDown || World->IsBeingCleanedUp()) return false;
	
	APlayerController* PlayerController = LocalPlayer->GetPlayerController(World);
	if (!IsValid(PlayerController) || PlayerController->IsActorBeingDestroyed()) return false;
	
	if (!LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->GetGameViewportWidget().IsValid()) return false;
	
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
	AppliedInputController = PlayerController;
	InputModeChanged(Mode);

	if (bFlushInput)
	{
		PlayerController->FlushPressedKeys();
	}
	return true;
}

void UPlayerInputSubsystem::SetWidgetToFocus(UWidget* WidgetToFocus)
{
	if (!ensureMsgf(WidgetToFocus, TEXT("Cannot focus a null widget."))) return;
	
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!ensureMsgf(LocalPlayer, TEXT("Input subsystem has no local player to focus a widget."))) return;
	
	if (!ensureMsgf(WidgetToFocus->GetOwningLocalPlayer() == LocalPlayer,
		TEXT("Cannot focus a widget owned by a different local player."))) return;
	
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
	MappingRequests.Reset();
	ApplyMappingRequests();
	OriginalContexts.Reset();
	ManagedContexts.Reset();
	PolicyRequests.Reset();
	AppliedInputController.Reset();
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

FGuid UPlayerInputSubsystem::AcquireMappingContext(UObject* Owner, UInputMappingContext* Context, int32 Priority, bool bGameplay)
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	auto* Enhanced = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!IsValid(Owner) || !Context || !Enhanced || Priority < 0) return FGuid();
	if (!OriginalContexts.Contains(Context))
	{
		int32 PreviousPriority = 0;
		if (Enhanced->HasMappingContext(Context, PreviousPriority)) OriginalContexts.Add(Context, PreviousPriority);
		else OriginalContexts.Add(Context, INDEX_NONE);
	}
	FPlayerMappingRequest& Request = MappingRequests.AddDefaulted_GetRef();
	Request.Handle = FGuid::NewGuid(); Request.Owner = Owner; Request.Context = Context;
	Request.Priority = Priority; Request.bGameplay = bGameplay;
	const FGuid Handle = Request.Handle;
	ApplyMappingRequests();
	return Handle;
}

void UPlayerInputSubsystem::ReleaseMappingContext(FGuid Handle)
{
	if (!Handle.IsValid()) return;
	MappingRequests.RemoveAll([Handle](const auto& Request) { return Request.Handle == Handle || !Request.Owner.IsValid(); });
	ApplyMappingRequests();
}

void UPlayerInputSubsystem::ApplyMappingRequests()
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	auto* Enhanced = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Enhanced) return;
	MappingRequests.RemoveAll([](const auto& Request) { return !Request.Owner.IsValid(); });
	TMap<UInputMappingContext*, int32> Desired;
	for (const auto& Request : MappingRequests)
		if (Request.Context && (!Request.bGameplay || !bGameplayBlocked))
		{
			int32* Existing = Desired.Find(Request.Context);
			if (Existing) *Existing = FMath::Max(*Existing, Request.Priority);
			else Desired.Add(Request.Context, Request.Priority);
		}
	FModifyContextOptions Options;
	Options.bIgnoreAllPressedKeysUntilRelease = true;
	TArray<TWeakObjectPtr<UInputMappingContext>> Finished;
	for (const auto& Entry : OriginalContexts)
	{
		UInputMappingContext* Context = Entry.Key.Get();
		if (!Context) continue;
		const bool bHasOwner = MappingRequests.ContainsByPredicate([Context](const auto& Request) { return Request.Context == Context; });
		if (const int32* Priority = Desired.Find(Context))
		{
			int32 CurrentPriority = 0;
			const int32 Effective = FMath::Max(*Priority, Entry.Value);
			if (!Enhanced->HasMappingContext(Context, CurrentPriority) || CurrentPriority != Effective)
				Enhanced->AddMappingContext(Context, Effective, Options);
			ManagedContexts.Add(Context);
		}
		else if (ManagedContexts.Contains(Context))
		{
			Enhanced->RemoveMappingContext(Context, Options);
			if (Entry.Value != INDEX_NONE) Enhanced->AddMappingContext(Context, Entry.Value, Options);
			ManagedContexts.Remove(Context);
		}
		if (!bHasOwner) Finished.Add(Context);
	}
	for (const auto& Context : Finished) OriginalContexts.Remove(Context);
}

FGuid UPlayerInputSubsystem::AcquireInputPolicy(UObject* Owner, FPlayerInputPolicy Policy)
{
	if (!IsValid(Owner)) return FGuid();
	if (PolicyRequests.IsEmpty()) { PolicyBaseMode = InputMode; PolicyBaseCursor = GamepadInputMode; }
	FPlayerPolicyRequest& Request = PolicyRequests.AddDefaulted_GetRef();
	Request.Handle = FGuid::NewGuid(); Request.Owner = Owner; Request.Policy = Policy;
	const FGuid Handle = Request.Handle;
	ApplyPolicyRequests();
	return Handle;
}

void UPlayerInputSubsystem::ReleaseInputPolicy(FGuid Handle)
{
	if (!Handle.IsValid()) return;
	PolicyRequests.RemoveAll([Handle](const auto& Request) { return Request.Handle == Handle || !Request.Owner.IsValid(); });
	ApplyPolicyRequests();
}

void UPlayerInputSubsystem::ApplyPolicyRequests()
{
	PolicyRequests.RemoveAll([](const auto& Request) { return !Request.Owner.IsValid(); });
	const UWorld* World = GetWorld();
	EInputMode Mode = PolicyBaseMode;
	EGamepadCursorMode Cursor = PolicyBaseCursor;
	bool bBlocked = false;
	for (const auto& Request : PolicyRequests)
	{
		bBlocked |= Request.Policy.bBlockGameplay;
		if (Request.Policy.bSetInputMode) Mode = Request.Policy.InputMode;
		if (Request.Policy.bSetCursorMode) Cursor = Request.Policy.CursorMode;
	}
	if (!IsValid(World) || World->bIsTearingDown || World->IsBeingCleanedUp())
	{
		// Preserve the logical restoration without touching a disappearing controller/viewport.
		bGameplayBlocked = bBlocked;
		InputMode = Mode;
		GamepadInputMode = Cursor;
		AppliedInputController.Reset();
		if (PreprocessorManager) PreprocessorManager->SetInputMode(Mode);
		return;
	}
	if (bGameplayBlocked != bBlocked)
	{
		bGameplayBlocked = bBlocked;
		// Release gameplay before switching focus/mappings. Each consumer verifies its current Pawn.
		OnGameplayInputBlockedChanged.Broadcast(bBlocked);
		ApplyMappingRequests();
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
	if (InputMode != Mode || AppliedInputController.Get() != PlayerController) SetPlayerInputMode(Mode, true);
	SetGamepadCursorMode(Cursor);
}

bool UPlayerInputSubsystem::IsKeyMappedToAction(FKey Key, const UInputAction* Action) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const auto* Enhanced = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return Action && Enhanced && Enhanced->QueryKeysMappedToAction(Action).Contains(Key);
}
