#include "HFSMStateComponent.h"
#include "HFSMState.h"
#include "Engine/InputDelegateBinding.h"


void UHFSMStateComponent::Init(UHFSMState* StateOwner)
{
	State = StateOwner;
	World = State->GetWorld();
}

void UHFSMStateComponent::Enter()
{
	if(const APlayerController* PC  = World->GetFirstPlayerController())
	{
		UInputDelegateBinding::BindInputDelegates(GetClass(), PC->InputComponent, this);		
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
