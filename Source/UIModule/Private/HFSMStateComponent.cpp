#include "HFSMStateComponent.h"
#include "HFSMState.h"
#include "HFSMLibrary.h"
#include "Components/InputComponent.h"


void UHFSMStateComponent::Init(UHFSMState* StateOwner)
{
	State = StateOwner;
	World = State ? State->GetWorld() : nullptr;
}

void UHFSMStateComponent::Enter()
{
	ResetBlueprintInput();
	if (!ensureMsgf(IsValid(State) && IsValid(GetContextOwner()), TEXT("State component has no valid context owner.")))
	{
		return;
	}
	World = GetWorld();
	if (!ensureMsgf(World, TEXT("State component has no world.")))
	{
		return;
	}
	BlueprintInputComponent = UHFSMLibrary::BindBlueprintInput(this, GetContextOwner());
	OnEnter();
}

void UHFSMStateComponent::Tick(float DeltaTime)
{
	OnTick(DeltaTime);
}

void UHFSMStateComponent::Exit(FName Event)
{
	ResetBlueprintInput();
	OnExit(Event);
}

void UHFSMStateComponent::BeginDestroy()
{
	ResetBlueprintInput();
	Super::BeginDestroy();
}

bool UHFSMStateComponent::IsTickEnabled()
{
	return bIsTickEnabled;
}

void UHFSMStateComponent::ResetBlueprintInput()
{
	UHFSMLibrary::UnbindBlueprintInput(BlueprintInputComponent);
	BlueprintInputComponent = nullptr;
}

UHFSMState* UHFSMStateComponent::GetState() const
{
	return State;
}

UObject* UHFSMStateComponent::GetContextOwner() const
{
	return IsValid(State) ? State->GetContextOwner() : nullptr;
}

APlayerController* UHFSMStateComponent::GetPlayerController() const
{ return IsValid(State) ? State->GetPlayerController() : nullptr; }
ULocalPlayer* UHFSMStateComponent::GetLocalPlayer() const
{ return IsValid(State) ? State->GetLocalPlayer() : nullptr; }

UWorld* UHFSMStateComponent::GetWorld() const
{
	return IsValid(State) ? State->GetWorld() : nullptr;
}
