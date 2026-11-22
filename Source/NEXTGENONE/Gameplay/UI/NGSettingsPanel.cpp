#include "Gameplay/UI/NGSettingsPanel.h"

#include "GameFramework/PlayerController.h"
#include "HFSMState.h"
#include "Input/PlayerInputSubsystem.h"

FReply UNGSettingsPanel::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (!Event.IsRepeat())
	{
		if (const auto* Input = UPlayerInputSubsystem::GetInputSubsystem(this))
		{
			for (const UInputAction* Action : CloseActions)
			{
				if (Input->IsKeyMappedToAction(Event.GetKey(), Action))
				{
					Cancel();
					return FReply::Handled();
				}
			}
		}
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

UNGSettingsScreenMounter::UNGSettingsScreenMounter()
{
	WidgetClass = UNGSettingsPanel::StaticClass();
}

void UNGSettingsScreenMounter::Enter()
{
	Super::Enter();

	auto* Panel = Cast<UNGGameSettingsScreen>(GetWidget());
	auto* Controller = GetPlayerController();
	auto* Input = UPlayerInputSubsystem::GetInputSubsystem(this);
	if (!Panel || !Controller || !Input)
	{
		Super::Exit(NAME_None);
		return;
	}

	FPlayerInputPolicy Policy;
	Policy.InputMode = EInputMode::UI;
	Policy.bBlockGameplay = true;
	Policy.bSetCursorMode = true;
	Policy.CursorMode = EGamepadCursorMode::Grid;

	AppliedInput = Input;
	PolicyHandle = Input->AcquireInputPolicy(this, Policy);
	Panel->OnBack.AddUniqueDynamic(this, &ThisClass::HandleBack);
	Panel->SetUserFocus(Controller);
	Input->SetWidgetToFocus(Panel);
}

void UNGSettingsScreenMounter::Exit(FName Event)
{
	if (auto* Panel = Cast<UNGGameSettingsScreen>(GetWidget()))
	{
		Panel->OnBack.RemoveDynamic(this, &ThisClass::HandleBack);
	}
	if (auto* Input = AppliedInput.Get())
	{
		Input->ReleaseInputPolicy(PolicyHandle);
	}
	PolicyHandle.Invalidate();
	AppliedInput.Reset();
	Super::Exit(Event);
}

void UNGSettingsScreenMounter::HandleBack()
{
	if (auto* OwningState = GetState())
	{
		OwningState->TriggerTransitionEvent(TEXT("Back"));
	}
}
