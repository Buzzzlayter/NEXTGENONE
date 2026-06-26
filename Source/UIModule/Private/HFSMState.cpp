#include "HFSMState.h"

#include "GameplayTagContainer.h"
#include "Algo/Transform.h"
#include "GCFSMUtilities.h"
#include "HFSMStateComponent.h"

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
	const static FName FSMsMapPropertyName = TEXT("FSMs");

	TArray<UGCFSM*> InternalFSMs;
	const FProperty* Property = GetClass()->FindPropertyByName(FSMsMapPropertyName);
	const TMap<FName, UGCFSM*>* Map = Property->ContainerPtrToValuePtr<TMap<FName, UGCFSM*>>(this);
	Map->GenerateValueArray(InternalFSMs);

	TArray<UHFSMState*> Output;
	Algo::TransformIf(
		InternalFSMs,
		Output,
		[](UGCFSM* FSM)
		{
			return FSM->GetActiveState()->IsA<UHFSMState>();
		},
		[](UGCFSM* FSM)
		{
			return Cast<UHFSMState>(FSM->GetActiveState());
		});

	return Output;
}

TArray<UHFSMStateComponent*> UHFSMState::GetComponents()
{
	return Components;
}

void UHFSMState::Enter()
{
	Super::Enter();

	for (UHFSMStateComponent* Component : Components)
	{
		if (Component)
		{
			Component->Init(this);
			Component->Enter();
		}
	}

	LaunchChildFSMs();
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
	for (int32 idx = Components.Num() - 1; idx >= 0; --idx)
	{
		if (UHFSMStateComponent* Component = Components[idx])
		{
			Component->Exit(event);
		}
	}

	Super::Exit(event);
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

void UHFSMState::MultiTransition(UObject* stateOrContext, TArray<FGameplayTag> Events)
{
	for (const FGameplayTag& Event : Events)
	{
		SimpleTransition(stateOrContext, Event);
	}
}

void UHFSMState::SimpleTransition(UObject* stateOrContext, FGameplayTag Event)
{
	UGCFSMUtilities::TriggerEvent(
		stateOrContext,
		Event.GetTagName(),
		EGCFSMTriggerEventTargetPolicy::ContextObject,
		true,
		false,
		EGCFSMTriggerEventQueuePolicy::JustQueue);
}
