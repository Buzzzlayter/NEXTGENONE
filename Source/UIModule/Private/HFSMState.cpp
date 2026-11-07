#include "HFSMState.h"

#include "HFSMStateComponent.h"
#include "HFSMLibrary.h"

APlayerController* UHFSMState::GetPlayerController() const
{
	return UHFSMLibrary::ResolvePlayerController(GetContextOwner());
}
ULocalPlayer* UHFSMState::GetLocalPlayer() const
{
	return UHFSMLibrary::ResolveLocalPlayer(GetContextOwner());
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

TArray<UHFSMState*> UHFSMState::GetChildren() const
{
	TArray<UGCFSM*> InternalFSMs;
	GetChildFSMs(InternalFSMs);

	TArray<UHFSMState*> Output;
	for (const UGCFSM* FSM : InternalFSMs)
	{
		if (!FSM) continue;
		
		if (UHFSMState* ChildState = Cast<UHFSMState>(FSM->GetActiveState()))
		{
			Output.Add(ChildState);
		}
	}

	return Output;
}

void UHFSMState::Enter()
{
	UHFSMLibrary::UnbindBlueprintInput(BlueprintInputComponent);
	BlueprintInputComponent = nullptr;
	for (UHFSMStateComponent* Component : Components)
	{
		if (Component)
		{
			Component->Init(this);
			Component->Enter();
		}
	}

	BlueprintInputComponent = UHFSMLibrary::BindBlueprintInput(this, GetContextOwner());
	Super::Enter();
}

void UHFSMState::Tick(float deltaTime)
{
	for (UHFSMStateComponent* Component : Components)
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
	UHFSMLibrary::UnbindBlueprintInput(BlueprintInputComponent);
	BlueprintInputComponent = nullptr;
	for (int32 idx = Components.Num() - 1; idx >= 0; --idx)
	{
		if (UHFSMStateComponent* Component = Components[idx])
		{
			Component->Exit(event);
			UHFSMLibrary::UnbindBlueprintInput(Component->BlueprintInputComponent);
			Component->BlueprintInputComponent = nullptr;
		}
	}
	Super::Exit(event);
}

void UHFSMState::BeginDestroy()
{
	UHFSMLibrary::UnbindBlueprintInput(BlueprintInputComponent);
	BlueprintInputComponent = nullptr;
	for (auto* Component : Components)
		if (IsValid(Component))
		{
			UHFSMLibrary::UnbindBlueprintInput(Component->BlueprintInputComponent);
			Component->BlueprintInputComponent = nullptr;
		}
	Super::BeginDestroy();
}

bool UHFSMState::GetClosestComponentInHierarchyUp(
	TSubclassOf<UHFSMStateComponent> ComponentClass,
	UHFSMStateComponent*& OutComponent)
{
	OutComponent = nullptr;
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
