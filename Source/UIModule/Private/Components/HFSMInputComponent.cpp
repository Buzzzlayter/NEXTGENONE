#include "Components/HFSMInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Input/PlayerInputSubsystem.h"

bool UHFSMInputComponent::Enter()
{
    if (!Super::Enter()) return false;
    UPlayerInputSubsystem* Input = GetPlayerInputSubsystem();
    if (!Input) { Super::Exit(NAME_None); return false; }
    AppliedInputSubsystem = Input;
    if (bSetupMappingContext)
    {
        MappingHandle = Input->AcquireMappingContext(this, MappingContext, Priority, false);
        if (!MappingHandle.IsValid()) { RestoreInput(); Super::Exit(NAME_None); return false; }
    }
    FPlayerInputPolicy Policy;
    Policy.bSetInputMode = bSetupInputMode; Policy.InputMode = InputMode;
    Policy.bSetCursorMode = bSetupCursorMode; Policy.CursorMode = DefaultCursorMode;
    Policy.bBlockGameplay = bBlockGameplayInput || (bSetupInputMode && InputMode == EInputMode::UI);
    PolicyHandle = Input->AcquireInputPolicy(this, Policy);
    return true;
}
void UHFSMInputComponent::Exit(FName Event) { RestoreInput(); Super::Exit(Event); }
void UHFSMInputComponent::AbortEnter() { RestoreInput(); Super::Exit(NAME_None); }
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
