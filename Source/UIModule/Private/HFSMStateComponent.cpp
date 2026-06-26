#include "HFSMStateComponent.h"
#include "HFSMState.h"
#include "Engine/InputDelegateBinding.h"


void UHFSMStateComponent::Init(UHFSMState* StateOwner)
{
	State = StateOwner;
	World = State ? State->GetWorld() : nullptr;
}

void UHFSMStateComponent::Enter()
{
	if (World)
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (PC->InputComponent)
			{
				UInputDelegateBinding::BindInputDelegates(GetClass(), PC->InputComponent, this);
			}
		}
	}
	OnEnter();
}

void UHFSMStateComponent::Tick(float DeltaTime)
{
	OnTick(DeltaTime);
}

void UHFSMStateComponent::Exit(FName Event)
{
	OnExit(Event);
}

void UHFSMStateComponent::PostInitProperties()
{
	if (const UObject* Outer = GetOuter())
		World = Outer->GetWorld();

	Super::PostInitProperties();
}

bool UHFSMStateComponent::IsTickEnabled()
{
	return bIsTickEnabled;
}

UHFSMState* UHFSMStateComponent::GetState() const
{
	return State;
}

UWorld* UHFSMStateComponent::GetWorld() const
{
	return World;
}
