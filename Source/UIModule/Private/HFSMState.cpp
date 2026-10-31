#include "HFSMState.h"

#include "HFSMStateComponent.h"

UHFSMStateComponent* UHFSMState::GetComponent(TSubclassOf<UHFSMStateComponent> ComponentClass) const
{
	const UClass* TargetClass = ComponentClass.Get();
	if (!TargetClass)
	{
		return nullptr;
	}

	for (UHFSMStateComponent* Component : Components)
	{
		if (Component && Component->IsA(TargetClass))
		{
			return Component;
		}
	}
	return nullptr;
}

TArray<UHFSMStateComponent*> UHFSMState::GetComponents(TSubclassOf<UHFSMStateComponent> ComponentClass) const
{
	TArray<UHFSMStateComponent*> Output;
	const UClass* TargetClass = ComponentClass.Get();
	if (!TargetClass)
	{
		return Output;
	}

	for (UHFSMStateComponent* Component : Components)
	{
		if (Component && Component->IsA(TargetClass))
		{
			Output.Add(Component);
		}
	}
	return Output;
}


UHFSMState* UHFSMState::GetRoot()
{
	UHFSMState* Parent = GetParent();
	return Parent ? Parent->GetRoot() : this;
}

UHFSMState* UHFSMState::GetParent() const
{
	UGCFSMState* ParentState = GetParentFSM()->GetContainerState();
	return Cast<UHFSMState>(ParentState);
}

TArray<UHFSMState*> UHFSMState::GetChildren()
{
	TArray<UGCFSM*> InternalFSMs;
	GetChildFSMs(InternalFSMs);

	TArray<UHFSMState*> Output;
	for (UGCFSM* FSM : InternalFSMs)
	{
		if (!FSM)
		{
			continue;
		}

		if (UHFSMState* ChildState = Cast<UHFSMState>(FSM->GetActiveState()))
		{
			Output.Add(ChildState);
		}
	}

	return Output;
}

TArray<UHFSMStateComponent*> UHFSMState::GetComponents()
{
	return Components;
}

void UHFSMState::Enter()
{
	bEnteredSuccessfully = false;
	ActiveComponents.Reset();
	for (UHFSMStateComponent* Component : Components)
	{
		if (Component)
		{
			Component->Init(this);
			if (!Component->Enter())
			{
				for (int32 Index = ActiveComponents.Num() - 1; Index >= 0; --Index)
				{
					ActiveComponents[Index]->AbortEnter();
				}
				ActiveComponents.Reset();
				OnEnterFailed(Component);
				return;
			}
			ActiveComponents.Add(Component);
		}
	}

	bEnteredSuccessfully = true;
	Super::Enter();
	LaunchChildFSMs();
}

void UHFSMState::Tick(float deltaTime)
{
	if (!bEnteredSuccessfully)
	{
		return;
	}
	for (UHFSMStateComponent* Component : ActiveComponents)
	{
		if (Component && Component->IsTickEnabled())
		{
			Component->Tick(deltaTime);
		}
	}

	Super::Tick(deltaTime);
}

void UHFSMState::Exit(FName event)
{
	for (int32 idx = ActiveComponents.Num() - 1; idx >= 0; --idx)
	{
		if (UHFSMStateComponent* Component = ActiveComponents[idx])
		{
			Component->Exit(event);
		}
	}
	ActiveComponents.Reset();
	if (bEnteredSuccessfully)
	{
		bEnteredSuccessfully = false;
		Super::Exit(event);
	}
}

bool UHFSMState::GetClosestComponentInHierarchyUp(
	TSubclassOf<UHFSMStateComponent> ComponentClass,
	UHFSMStateComponent*& OutComponent)
{
	if (const UClass* TargetClass = ComponentClass.Get())
	{
		IterateUpConditional(
			[TargetClass, &OutComponent](const UHFSMState* State)
			{
				const int32 Idx = State->Components.FindLastByPredicate(
					[TargetClass](const UHFSMStateComponent* Component)
					{
						return Component ? Component->IsA(TargetClass) : false;
					});
				if (State->Components.IsValidIndex(Idx))
				{
					OutComponent = State->Components[Idx];
					return false;
				}

				return true;
			});
	}
	return IsValid(OutComponent);
}

void UHFSMState::IterateUpConditional(TFunctionRef<bool(UHFSMState*)> Func)
{
	for (UHFSMState* State = this; State; State = State->GetParent())
	{
		if (!Func(State))
		{
			return;
		}
	}
}

void UHFSMState::IterateDownConditional(TFunctionRef<bool(UHFSMState*)> Func)
{
	if (!Func(this))
	{
		return;
	}

	for (UHFSMState* Child : GetChildren())
	{
		Child->IterateDownConditional(Func);
	}
}
