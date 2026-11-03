#include "Components/HFSMInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Input/PlayerInputSubsystem.h"

void UHFSMInputComponent::Enter()
{
    AppliedInputSubsystem = GetPlayerInputSubsystem();
    if (bSetupMappingContext)
    {
        MappingHandle = AppliedInputSubsystem->AcquireMappingContext(this, MappingContext, Priority, false);
        if (!MappingHandle.IsValid()) { RestoreInput(); return; }
    }
    FPlayerInputPolicy Policy;
    Policy.bSetInputMode = bSetupInputMode; Policy.InputMode = InputMode;
    Policy.bSetCursorMode = bSetupCursorMode; Policy.CursorMode = DefaultCursorMode;
    Policy.bBlockGameplay = bBlockGameplayInput || (bSetupInputMode && InputMode == EInputMode::UI);
    PolicyHandle = AppliedInputSubsystem->AcquireInputPolicy(this, Policy);
    Super::Enter();
}
void UHFSMInputComponent::Exit(FName Event)
{
    RestoreInput(); Super::Exit(Event);
}
void UHFSMInputComponent::RestoreInput()
{
    if (UPlayerInputSubsystem* Input = AppliedInputSubsystem.Get())
    {
        Input->ReleaseMappingContext(MappingHandle);
        Input->ReleaseInputPolicy(PolicyHandle);
    }
    MappingHandle.Invalidate(); PolicyHandle.Invalidate(); AppliedInputSubsystem.Reset();
}
UPlayerInputSubsystem* UHFSMInputComponent::GetPlayerInputSubsystem() const
{
    const ULocalPlayer* Player = GetLocalPlayer();
    return Player ? Player->GetSubsystem<UPlayerInputSubsystem>() : nullptr;
}
