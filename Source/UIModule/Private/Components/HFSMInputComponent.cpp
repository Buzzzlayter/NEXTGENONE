
#include "Components/HFSMInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Input/PlayerInputSubsystem.h"
#include "UIModule.h"

void UHFSMInputComponent::Enter()
{
	Super::Enter();

	if (bSetupInputMode)
	{
		UPlayerInputSubsystem::SetInputMode(GetWorld(), InputMode, bFlushInput);
	}

	if (bSetupCursorMode)
	{
		const auto InputSubsystem = UPlayerInputSubsystem::GetInputSubsystem(GetWorld());
		GENCHECK(InputSubsystem, "UHFSMInputComponent::Enter failed. InputSubsystem is null.");
		InputSubsystem->SetGamepadCursorMode(DefaultCursorMode);
	}

	if (bSetupMappingContext)
	{
		GENCHECK(MappingContext && Priority >= 0, "UHFSMInputComponent::Enter failed. MappingContext is null or Priority is invalid.");

		const auto EnhancedInputSubsystem = GetEnhancedInputSubsystem();
		GENCHECK(EnhancedInputSubsystem, "UHFSMInputComponent::Enter failed. EnhancedInputSubsystem is null.");

		EnhancedInputSubsystem->AddMappingContext(MappingContext, Priority);

		UE_LOG(LogPlayerInputSubsystem,
			Log,
			TEXT("Mapping context added: Name  = %s, Priority = %d"),
			*MappingContext->GetName(),
			Priority);
	}
}

void UHFSMInputComponent::Exit(FName Event)
{
	if (bSetupMappingContext)
	{
		const auto EnhancedInputSubsystem = GetEnhancedInputSubsystem();
		if (EnhancedInputSubsystem && MappingContext)
		{
			EnhancedInputSubsystem->RemoveMappingContext(MappingContext);

			UE_LOG(LogPlayerInputSubsystem, Log, TEXT("Mapping context removed: Name  = %s"), *MappingContext->GetName());
		}
	}

	Super::Exit(Event);
}

UEnhancedInputLocalPlayerSubsystem* UHFSMInputComponent::GetEnhancedInputSubsystem() const
{
	const UWorld* world = GetWorld();
	if (!world)
	{
		return nullptr;
	}

	const ULocalPlayer* LocalPlayer = world->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
}
