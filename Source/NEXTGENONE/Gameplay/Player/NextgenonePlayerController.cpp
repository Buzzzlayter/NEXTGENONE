#include "Gameplay/Player/NextgenonePlayerController.h"
#include "Character/ALSBaseCharacter.h"
#include "NGGameUserSettings.h"
#include "Input/NGGameplayInputComponent.h"
#include "Components/ALSDebugComponent.h"

void ANextgenonePlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController())
    {
        if (auto* Settings = UNGGameUserSettings::Get())
        {
            Settings->RunStartupBenchmark();
            Settings->ApplyForPlayer(*this);
        }
    }
}
void ANextgenonePlayerController::SetupInputComponent()
{
    // ALS auto-binds by action name and adds its own contexts. The Pawn now owns gameplay bindings.
    APlayerController::SetupInputComponent();
}
void ANextgenonePlayerController::RefreshPawnPresentation()
{
	if (APawn* PriorGameplayPawn = PreviousInputPawn.Get(); PriorGameplayPawn && PriorGameplayPawn != GetPawn())
		if (auto* Input = PriorGameplayPawn->FindComponentByClass<UNGGameplayInputComponent>()) Input->ShutdownInput();
	PreviousInputPawn = GetPawn();
    PossessedCharacter = Cast<AALSBaseCharacter>(GetPawn());
    if (GetNetMode() != NM_DedicatedServer) SetupCamera();
    if (PossessedCharacter)
        if (auto* Debug = PossessedCharacter->FindComponentByClass<UALSDebugComponent>()) Debug->OnPlayerControllerInitialized(this);
}
void ANextgenonePlayerController::OnPossess(APawn* ControlledPawn)
{
    APlayerController::OnPossess(ControlledPawn);
    RefreshPawnPresentation();
}
void ANextgenonePlayerController::OnRep_Pawn()
{
    APlayerController::OnRep_Pawn();
    RefreshPawnPresentation();
}
