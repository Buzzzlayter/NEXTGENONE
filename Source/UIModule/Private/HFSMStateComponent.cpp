#include "HFSMStateComponent.h"
#include "HFSMState.h"


void UHFSMStateComponent::Init(UHFSMState* StateOwner)
{
	State = StateOwner;
	World = State ? State->GetWorld() : nullptr;
}

bool UHFSMStateComponent::Enter()
{
	if (!ensureMsgf(IsValid(State) && IsValid(GetContextOwner()), TEXT("State component has no valid context owner.")))
	{
		return false;
	}
	World = GetWorld();
	if (!ensureMsgf(World, TEXT("State component has no world.")))
	{
		return false;
	}
	OnEnter();
	return true;
}

void UHFSMStateComponent::Tick(float DeltaTime)
{
	OnTick(DeltaTime);
}

void UHFSMStateComponent::Exit(FName Event)
{
	OnExit(Event);
}

void UHFSMStateComponent::AbortEnter()
{
	Exit(NAME_None);
}

bool UHFSMStateComponent::IsTickEnabled()
{
	return bIsTickEnabled;
}

UHFSMState* UHFSMStateComponent::GetState() const
{
	return State;
}

UObject* UHFSMStateComponent::GetContextOwner() const
{
	return IsValid(State) ? State->GetContextOwner() : nullptr;
}

UWorld* UHFSMStateComponent::GetWorld() const
{
	return IsValid(State) ? State->GetWorld() : nullptr;
}
