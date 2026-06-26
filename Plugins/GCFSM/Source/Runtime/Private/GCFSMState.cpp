// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMState.h"
#include "GCFSMSubmachineState.h"
#include "GCFSMModule.h"
#include "GCFSM.h"
#include "GCFSMRootState.h"
#include "GCFSMConstants.h"
#include "GCFSMUtilities.h"
#include "GCFSMReplicationComponent.h"
#include "GameFramework/Actor.h"

float UGCFSMState::GetRootTime() const
{
	return rootState->GetActiveTime();
}

bool UGCFSMState::IsRootState() const
{
	return rootState == this;
}

template <class T>
T makeCopy(const T& value)
{
	return value;
}

UGCFSMState* UGCFSMState::MakeStateObject(UGCFSM* fsm, TSubclassOf<UGCFSMState> implementationClass, bool processTickEvent, float timeOut, const FString& stateName)
{
	check(fsm);
	check(implementationClass);
	check(implementationClass->IsChildOf(UGCFSMState::StaticClass()));

#if WITH_EDITOR
	FString objName = FString::Printf(TEXT("%s/%s/%s"), *fsm->GetContainerState()->GetContextObject()->GetName(), *fsm->GetNameForLogging(), *stateName);
	auto newState = NewObject<UGCFSMState>(fsm, implementationClass, *objName);
#else
	auto newState = NewObject<UGCFSMState>(fsm, implementationClass);
#endif
	newState->Initialize(nullptr, fsm->GetContainerState(), processTickEvent, timeOut, stateName);
	return newState;
}

void UGCFSMState::Initialize(UObject* context, UGCFSMState* parent, bool processTickEvent, float timeOut, const FString& stateName)
{
	UObject* contextObjectPtr;

	if (context)
	{
		contextObjectPtr = context;
		rootState = Cast<UGCFSMRootState>(this);
	}
	else
	{
		check(parent);
		contextObjectPtr = parent->contextObject.Get();
		check(contextObjectPtr);
		rootState = parent->rootState;
	}

	contextObject = contextObjectPtr;

	// look for a property named "Context" and initialize it
	if (auto property = GetContextProperty(GetClass()))
	{
		if (contextObjectPtr->GetClass()->IsChildOf(property->PropertyClass))
		{
			property->SetObjectPropertyValue_InContainer(this, contextObjectPtr);
		}
		else
		{
			GC_LOG(Warning,
				TEXT("State %s has a Context property of class %s, but the actual Context object is of class %s"),
				*GetNameForLogging(),
				*property->PropertyClass->GetName(),
				*contextObjectPtr->GetClass()->GetName());
		}
	}
	UGCFSMBasicState::Initialize(processTickEvent, timeOut, stateName);
}

#if ENABLE_VISUAL_LOG
FString UGCFSMState::MakeDetailedNameForLogging() const
{
	return FString::Printf(TEXT("%s, class %s"), *GetNameForLogging(), *GetClass()->GetName());
}
#endif

FObjectProperty* UGCFSMState::GetContextProperty(UClass* klass)
{
	static FName context { TEXT("Context") };
	auto property = klass->FindPropertyByName(context);
	return property ? CastField<FObjectProperty>(property) : nullptr;
}

UClass* UGCFSMState::GetContextClassForClass(UClass* klass)
{
	auto property = GetContextProperty(klass);
	return property ? property->PropertyClass : nullptr;
}

void UGCFSMState::PrivateEnter()
{
	// call customization point
	Enter();
}

void UGCFSMState::PrivateExit(FName event)
{
	if (ShouldSaveHistoryBeforeExiting())
	{
		GetParentFSM()->SaveHistoryForActiveState(this);
	}

	// First of all, terminate all submachines
	for (auto& fsm : makeCopy(FSMs))
	{
		// Sub-FSMs do not get the event name
		fsm.Value->ExitActiveState(NAME_None);
		fsm.Value->Stop();
	}
	FSMs.Empty();

	// Finally, call customization point
	Exit(event);
}

UGCFSMBasicState::TickResult UGCFSMState::PrivateTick(float deltaTime)
{
	const float rootTime = GetRootTime();
	auto result = Super::PrivateTick(deltaTime);

	// call customization point
	Tick(deltaTime);

	for (auto& fsm : makeCopy(FSMs))
	{
		fsm.Value->Tick(rootTime, deltaTime);
	}

	return result;
}

UGCFSM* UGCFSMState::MakeFSM(FName fsmName, EGCFSMReplicationOptions replicationOptions, EGCFSMReplicatedExec& exec)
{
	UGCFSM* fsm = nullptr;
	exec = EGCFSMReplicatedExec::Proxy; // in case of error, it's safer to follow the Proxy route

	if (replicationOptions == EGCFSMReplicationOptions::NonReplicated)
	{
		fsm = NewObject<UGCFSM>(this, fsmName);
		FSMs.Add(fsmName, fsm);
		if (restoringSnapshot)
		{
			GC_VLOG(GetContextObject(), Log, TEXT("FSM %s: started (non-replicated, restoring snapshot)"), *fsm->GetNameForLogging());
		}
		else
		{
			exec = EGCFSMReplicatedExec::AuthorityOrLocal;
			GC_VLOG(GetContextObject(), Log, TEXT("FSM %s: started (non-replicated)"), *fsm->GetNameForLogging());
		}
	}
	else if (auto actor = Cast<AActor>(GetContextObject()))
	{
		auto parentFSM = GetParentFSM();
		if (parentFSM && parentFSM->GetRole() == EGCFSMRole::Local)
		{
			GC_LOG(Error, TEXT("Launch of replicated FSM failed: the parent FSM %s is not replicated"), *parentFSM->GetNameForLogging());
		}
		else if (!actor->GetIsReplicated())
		{
			GC_LOG(Error, TEXT("Launch of replicated FSM failed: the context actor %s is not replicated"), *actor->GetName());
		}
		else if (auto component = actor->FindComponentByClass<UGCFSMReplicationComponent>())
		{
			fsm = NewObject<UGCFSM>(this, fsmName);
			FSMs.Add(fsmName, fsm);
			fsm->replicationComponent = component;
			if (actor->HasAuthority())
			{
				fsm->SetRole(EGCFSMRole::Authority);
				exec = EGCFSMReplicatedExec::AuthorityOrLocal;
				GC_VLOG(GetContextObject(), Log, TEXT("FSM %s: started on server (replicated)"), *fsm->GetNameForLogging());
			}
			else
			{
				fsm->SetRole(EGCFSMRole::Proxy);
				GC_VLOG(GetContextObject(), Log, TEXT("FSM %s: started on client (replicated)"), *fsm->GetNameForLogging());
			}
			check(parentFSM == nullptr || parentFSM->GetRole() == fsm->GetRole());
		}
		else
		{
			GC_LOG(Error, TEXT("Launch of replicated FSM failed: the context actor %s does not have a UGCFSMReplicationComponent"), *actor->GetName());
		}
	}
	else
	{
		GC_LOG(Error, TEXT("Launch of replicated FSM failed: the context object %s is not an actor"), GetContextObject() ? *GetContextObject()->GetName() : TEXT("<none>"));
	}
	return fsm;
}

bool UGCFSMState::IsProxy() const
{
	if (GetParentFSM())
	{
		return GetParentFSM()->GetRole() == EGCFSMRole::Proxy;
	}
	else if (auto actor = Cast<AActor>(GetContextObject()))
	{
		return actor->GetIsReplicated() && !actor->HasAuthority();
	}
	else
	{
		return false;
	}
}

void UGCFSMState::LaunchFSM(UObject* blueprintContext, FName fsmName, EGCFSMReplicationOptions replicationOptions)
{
	if (replicationOptions == EGCFSMReplicationOptions::NonReplicated && restoringSnapshot)
	{
		GC_LOG(Verbose, TEXT("In state %s, ignoring launch of FSM %s, since we are restoring a snapshot"), *GetNameForLogging(), *fsmName.ToString());
	}
	else if (auto pfsm = FSMs.Find(fsmName))
	{
		// don't warn about replicated FSMs on proxies, it's ok
		if (replicationOptions == EGCFSMReplicationOptions::NonReplicated || !IsProxy())
		{
			GC_LOG(Error, TEXT("FSM %s already started, new launch request ignored"), *(*pfsm)->GetNameForLogging());
		}
	}
	else if (!UGCFSM::InvokeEntryPoint(blueprintContext, fsmName, this, replicationOptions))
	{
		GC_LOG(Error, TEXT("In state %s, invalid FSM name %s in LaunchFSM call"), *GetNameForLogging(), *fsmName.ToString());
	}
}

bool UGCFSMState::IsFSMRunning(FName fsmName) const
{
	return FSMs.Find(fsmName) != nullptr;
}

void UGCFSMState::TriggerEvent(const FGCFSMTriggeredEvent& event)
{
	if (IsValid(this)) //[[likely]]
	{
		for (auto& fsm : FSMs)
		{
			fsm.Value->TriggerEvent(event);
		}
	}
	else
	{
		GC_LOG(Warning, TEXT("State %s is invalid, event %s ignored"), *GetNameForLogging(), *event.name.ToString());
	}

}

UFunction* UGCFSMState::GetInternalEventHandler(FName eventName)
{
	FString fullName { internalEventNamespacePrefix };
	eventName.AppendString(fullName);
	UObject* blueprintContext = rootState == this ? contextObject.Get() : static_cast<UObject*>(this);
	return blueprintContext->FindFunction(*fullName);
}

void UGCFSMState::RemoveFSM(FName name)
{
	FSMs.Remove(name);
}

UGCFSM* UGCFSMState::GetFSM(FName name) const
{
	auto pfsm = FSMs.Find(name);
	return pfsm ? *pfsm : nullptr;
}

void UGCFSMState::GetChildFSMs(TArray<UGCFSM*>& OutFSMs) const
{
	OutFSMs.Reset();
	OutFSMs.Reserve(FSMs.Num());

	for (const TPair<FName, TObjectPtr<UGCFSM>>& FSM : FSMs)
	{
		if (UGCFSM* ChildFSM = FSM.Value.Get())
		{
			OutFSMs.Add(ChildFSM);
		}
	}
}

UGCFSM* UGCFSMState::GetFSMRecursive(FName name) const
{
	auto result = GetFSM(name);
	if (result == nullptr)
	{
		for (auto& fsm : FSMs)
		{
			result = fsm.Value->GetActiveState()->GetSubmachineFSM(name);
			if (result)
			{
				break;
			}
		}
	}
	return result;
}

UGCFSM* UGCFSMState::ResolveFSMPathName(const TArray<FName>& fsmPathName, bool startIfNecessary, int startIdx)
{
	check(fsmPathName.Num() > startIdx);
	const FName& name = fsmPathName[startIdx];
	auto fsm = GetFSM(name);
	if (fsm == nullptr && startIfNecessary)
	{
		// no FSM with this name found, try launching it
		if (UGCFSM::InvokeEntryPoint(GetBlueprintContext(), name, this, EGCFSMReplicationOptions::Replicated))
		{
			fsm = FSMs.FindChecked(name);
		}
		else
		{
			GC_LOG(Error, TEXT("Could not start FSM %s in state object %s"), *name.ToString(), *GetNameForLogging());
		}
	}

	if (fsmPathName.Num() == startIdx + 1)
	{
		return fsm;
	}
	else if (fsm && fsm->GetActiveState())
	{
		return fsm->GetActiveState()->ResolveFSMPathName(fsmPathName, startIfNecessary, startIdx + 1);
	}
	else
	{
		return nullptr;
	}
}

void UGCFSMState::UpdateReplicatedFSM(const TArray<FName>& fsmPathName, FName exitEvent, const FGuid& stateGuid)
{
	if (auto fsm = ResolveFSMPathName(fsmPathName, true))
	{
		check(fsm->GetRole() == EGCFSMRole::Proxy);

		// exit current state, if any
		fsm->ReplicatedExitState(exitEvent);

		// enter new state, if provided, otherwise remain in the "wait" pseudostate
		if (stateGuid.IsValid())
		{
			fsm->ReplicatedEnterState(stateGuid);
		}
	}
}

void UGCFSMState::StopReplicatedFSM(const TArray<FName>& fsmPathName, FName exitEvent)
{
	if (auto fsm = ResolveFSMPathName(fsmPathName, false))
	{
		check(fsm->GetRole() == EGCFSMRole::Proxy);
		fsm->ReplicatedExitState(exitEvent);
		fsm->Stop();
	}
}

FString UGCFSMState::GetDebugInfo() const
{
	return GetFSMDebugInfo();
}


#if WITH_EDITOR
UGCFSM* UGCFSMState::GetActiveFSMForGraphNode(const FGuid& nodeGuid) const
{
	for (auto& fsm : FSMs)
	{
		if (fsm.Value->GetActiveNodeGuid() == nodeGuid)
		{
			return fsm.Value;
		}
		else if (auto submachine = Cast<UGCFSMSubmachineState>(fsm.Value->GetActiveState()))
		{
			if (auto subFsm = submachine->GetActiveFSMForGraphNode(nodeGuid))
			{
				return subFsm;
			}
		}
	}
	return nullptr;
}
#endif

void UGCFSMState::InternalEventEntryPoint(float)
{}

void UGCFSMState::MakeSnapshot(UGCFSMSnapshot::Builder& builder) const
{
	for (auto& fsm : FSMs)
	{
		fsm.Value->MakeSnapshot(builder);
	}
	builder.EndState();
}

namespace
{
	class ScopedFlag
	{
		bool& flag;

	public:
		ScopedFlag(bool& f) : flag{ f }
		{
			check(flag == false);
			flag = true;
		}

		~ScopedFlag()
		{
			check(flag == true);
			flag = false;
		}
	};
}

void UGCFSMState::RestoreSnapshot(UGCFSMSnapshot::Iterator& iterator)
{
	ScopedFlag flag{ restoringSnapshot };
	while (iterator)
	{
		const auto& element = *iterator;
		++iterator;

		if (element.fsmName.IsNone())
		{
			break;
		}

		check(element.activeNodeGuid.IsValid());
		auto fsm = GetFSM(element.fsmName);
		bool shouldEnterState = true;
		if (fsm)
		{
			// the FSMs is already running, check if the correct state is already active
			// since in that case we do not need to exit and re-enter the state
			if (fsm->GetActiveNodeGuid() == element.activeNodeGuid)
			{
				shouldEnterState = false;
				GC_VLOG(GetContextObject(), Log, TEXT("FSM %s: restoring snapshot while running (no state change)"), *fsm->GetNameForLogging());
			}
			else
			{
				GC_VLOG(GetContextObject(), Log, TEXT("FSM %s: restoring snapshot while running (state will change)"), *fsm->GetNameForLogging());
				fsm->ReplicatedExitState(NAME_None);
			}
		}
		else if (UGCFSM::InvokeEntryPoint(GetBlueprintContext(), element.fsmName, this, EGCFSMReplicationOptions::NonReplicated))
		{
			fsm = FSMs.FindChecked(element.fsmName);
		}

		if (fsm)
		{
			fsm->stateHistories = element.stateHistories;

			if (shouldEnterState)
			{
				fsm->ReplicatedEnterState(element.activeNodeGuid);
			}

			if (fsm->GetActiveState())
			{
				fsm->GetActiveState()->RestoreSnapshot(iterator);
				continue;
			}
		}

		GC_LOG(Error, TEXT("The FSM snapshot object does not match the FSM graph or is corrupted. Some FSMs could not be restored and the overall state may be inconsistent."));
		iterator.SetToEnd();
	}
}

UGCFSMSnapshot* UGCFSMState::MakeFSMSnapshot()
{
	UGCFSMSnapshot::Builder builder(this);
	MakeSnapshot(builder);
	return builder.MakeFSMSnapshot();
}

void UGCFSMState::RestoreFSMSnapshot(const UGCFSMSnapshot* snapshot)
{
	if (snapshot == nullptr)
	{
		GC_LOG(Error, TEXT("Invalid snapshot object"));
	}
	else if (restoringSnapshot)
	{
		GC_LOG(Error, TEXT("In state %s, RestoreFSMSnapshot call ignored, since we are already restoring a snapshot"), *GetNameForLogging());
	}
	else
	{
		auto iterator = snapshot->GetIterator();
		RestoreSnapshot(iterator);
	}
}
