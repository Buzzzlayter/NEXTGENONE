
#include "Components/HFSMInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Input/PlayerInputSubsystem.h"

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
		InputSubsystem->SetGamepadCursorMode(DefaultCursorMode);
	}

	if (bSetupMappingContext)
	{
		check(MappingContext && Priority >= 0);

		const auto EnhancedInputSubsystem = GetEnhancedInputSubsystem();
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
		EnhancedInputSubsystem->RemoveMappingContext(MappingContext);

		UE_LOG(LogPlayerInputSubsystem, Log, TEXT("Mapping context removed: Name  = %s"), *MappingContext->GetName());
	}

	Super::Exit(Event);
}

UEnhancedInputLocalPlayerSubsystem* UHFSMInputComponent::GetEnhancedInputSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
}
