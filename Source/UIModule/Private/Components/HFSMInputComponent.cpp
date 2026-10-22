
#include "Components/HFSMInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputMappingContext.h"
#include "Input/PlayerInputSubsystem.h"

bool UHFSMInputComponent::Enter()
{
	UPlayerInputSubsystem* InputSubsystem = GetPlayerInputSubsystem();
	if ((bSetupInputMode || bSetupCursorMode)
		&& !ensureMsgf(InputSubsystem, TEXT("Input component has no player input subsystem.")))
	{
		return false;
	}
	UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = nullptr;
	if (bSetupMappingContext)
	{
		if (!ensureMsgf(MappingContext && Priority >= 0, TEXT("Input component has an invalid mapping context or priority.")))
		{
			return false;
		}
		EnhancedInputSubsystem = GetEnhancedInputSubsystem();
		if (!ensureMsgf(EnhancedInputSubsystem, TEXT("Input component has no Enhanced Input subsystem.")))
		{
			return false;
		}
	}
	if (InputSubsystem)
	{
		PreviousInputMode = InputSubsystem->GetInputMode();
		PreviousCursorMode = InputSubsystem->GetGamepadCursorMode();
	}
	if (bSetupInputMode && !InputSubsystem->SetPlayerInputMode(InputMode, bFlushInput))
	{
		return false;
	}
	AppliedInputSubsystem = InputSubsystem;
	if (bSetupCursorMode)
	{
		InputSubsystem->SetGamepadCursorMode(DefaultCursorMode);
	}
	if (EnhancedInputSubsystem)
	{
		bHadMappingContext = EnhancedInputSubsystem->HasMappingContext(MappingContext, PreviousMappingPriority);
		EnhancedInputSubsystem->AddMappingContext(MappingContext, Priority);
		AppliedEnhancedInputSubsystem = EnhancedInputSubsystem;
		bAppliedMappingContext = true;
	}
	if (!Super::Enter())
	{
		RestoreInput();
		return false;
	}
	return true;
}

void UHFSMInputComponent::Exit(FName Event)
{
	if (bAppliedMappingContext)
	{
		const auto EnhancedInputSubsystem = AppliedEnhancedInputSubsystem.Get();
		if (EnhancedInputSubsystem && MappingContext)
		{
			EnhancedInputSubsystem->RemoveMappingContext(MappingContext);

			UE_LOG(LogPlayerInputSubsystem, Log, TEXT("Mapping context removed: Name  = %s"), *MappingContext->GetName());
		}
	}
	bAppliedMappingContext = false;
	AppliedEnhancedInputSubsystem.Reset();
	AppliedInputSubsystem.Reset();

	Super::Exit(Event);
}

void UHFSMInputComponent::AbortEnter()
{
	RestoreInput();
	Super::Exit(NAME_None);
}

void UHFSMInputComponent::RestoreInput()
{
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = AppliedEnhancedInputSubsystem.Get())
	{
		if (bAppliedMappingContext && MappingContext)
		{
			EnhancedInputSubsystem->RemoveMappingContext(MappingContext);
			if (bHadMappingContext)
			{
				EnhancedInputSubsystem->AddMappingContext(MappingContext, PreviousMappingPriority);
			}
		}
	}
	if (UPlayerInputSubsystem* InputSubsystem = AppliedInputSubsystem.Get())
	{
		if (bSetupInputMode)
		{
			InputSubsystem->SetPlayerInputMode(PreviousInputMode, false);
		}
		if (bSetupCursorMode)
		{
			InputSubsystem->SetGamepadCursorMode(PreviousCursorMode);
		}
	}
	bAppliedMappingContext = false;
	AppliedEnhancedInputSubsystem.Reset();
	AppliedInputSubsystem.Reset();
}

UPlayerInputSubsystem* UHFSMInputComponent::GetPlayerInputSubsystem() const
{
	const ULocalPlayer* Player = GetLocalPlayer();
	return Player ? Player->GetSubsystem<UPlayerInputSubsystem>() : nullptr;
}

UEnhancedInputLocalPlayerSubsystem* UHFSMInputComponent::GetEnhancedInputSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
}
