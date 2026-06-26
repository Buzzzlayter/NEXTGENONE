// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSM.h"
#include "GCFSMModule.h"
#include "GCFSMConstants.h"
#include "GCFSMState.h"
#include "GCFSMReplicationComponent.h"
#include "GCFSMSnapshot.h"
#include <utility>

#define GC_VLOG_FSM_MSG(Verbosity, Format, ...) \
	GC_VLOG(GetContainerState()->GetContextObject(), Verbosity, Format, *GetNameForLogging(), activeState ? *activeState->GetDetailedNameForLogging() : TEXT("<none>"), ##__VA_ARGS__)

#define GC_VLOG_FSM(Verbosity, Format, ...)	\
	GC_VLOG_FSM_MSG(Verbosity, TEXT("FSM %s (in state %s): ") Format, ##__VA_ARGS__)

void UGCFSM::LatentExecutor::Set(const FLatentActionInfo& latentInfo, FName& _eventName, float& _deltaTime)
{
	target = latentInfo.CallbackTarget;
	function = target->FindFunction(latentInfo.ExecutionFunction);
	linkage = latentInfo.Linkage;
	eventName = &_eventName;
	deltaTime = &_deltaTime;
	*eventName = UGCFSMBasicState::GetTickEventName();
}

void UGCFSM::LatentExecutor::ProcessEvent(FName name)
{
	*eventName = name;
	target->ProcessEvent(function, &linkage);
}

void UGCFSM::LatentExecutor::ProcessTick(float _deltaTime)
{
	check(deltaTime);
	*deltaTime = _deltaTime;
	target->ProcessEvent(function, &linkage);
}

void UGCFSM::EnterState(UGCFSMBasicState* state, FGuid nodeGuid, FLatentActionInfo latentInfo, FName& eventName, float& deltaTime, const TArray<FName>& _processedEvents, int32 _numTransitions)
{
	if (activeState)
	{
		GC_LOG(Error, TEXT("FSM %s tried to enter a state, while it is already running state %s"), *GetNameForLogging(), *activeState->GetName());
	}
	else
	{
		check(state);

		// replicate entering the state, if necessary, before calling the OnExit handlers, this ensure
		// that the replication is correctly ordered in case the OnEnter handlers launch one or more replicated sub-FSMs
		if (role == EGCFSMRole::Authority)
		{
			replicationComponent->MulticastUpdateFSM(GetPathName(), exitEventToReplicate, nodeGuid);
			exitEventToReplicate = NAME_None;
		}

		latentExecutor.Set(latentInfo, eventName, deltaTime);
		processedEvents = _processedEvents;
		numTransitions = _numTransitions;

		activeState = state;
		activeNodeGuid = nodeGuid;

		GC_VLOG_FSM_MSG(Log, TEXT("FSM %s: enters state %s"));
		state->PrivateEnter();

		// append all events currently being deferred to the event queue and clear the deferred queue
		eventQueue.Append(std::move(eventsBeingDeferred));
	}
}

void UGCFSM::ReplicationLatentExecutor::Set(const FLatentActionInfo& latentInfo, FName& _stateOrStop)
{
	target = latentInfo.CallbackTarget;
	function = target->FindFunction(latentInfo.ExecutionFunction);
	linkage = latentInfo.Linkage;
	stateOrStop = &_stateOrStop;
}

void UGCFSM::ReplicationLatentExecutor::Process(FName _stateOrStop)
{
	*stateOrStop = _stateOrStop;
	target->ProcessEvent(function, &linkage);
}

void UGCFSM::ReplicationWaitFunction(FName& stateOrStop, FLatentActionInfo latentInfo)
{
	replicationLatentExecutor.Set(latentInfo, stateOrStop);
}

void UGCFSM::SetRole(EGCFSMRole r)
{
	check(r != EGCFSMRole::Local);
	check(pathName.Num() == 0);
	role = r;
	if (role == EGCFSMRole::Authority)
	{
		auto state = GetContainerState();
		if (auto parentFSM = state->GetParentFSM())
		{
			pathName = parentFSM->GetPathName();
		}
		pathName.Add(GetFName());
	}
}

const TArray<FName>& UGCFSM::GetPathName() const
{
	check(role == EGCFSMRole::Authority);
	check(pathName.Num() > 0);
	return pathName;
}

UGCFSMState* UGCFSM::GetContainerState() const
{
	return static_cast<UGCFSMState*>(GetOuter());
}

float UGCFSM::GetActiveStateTime() const
{
	return activeState ? activeState->GetActiveTime() : 0;
}

void UGCFSM::ProcessTransition(FName event)
{
	ExitActiveState(event);
	check(!activeNodeGuid.IsValid());

	if (role == EGCFSMRole::Authority)
	{
		// keep memory about the exit event, it will be used the next time UpdateFSM is called
		// which could be either while evaluating ProcessEvent or in the lines below
		exitEventToReplicate = event;
	}

	latentExecutor.ProcessEvent(event);

	if (role == EGCFSMRole::Authority && !activeState && IsValid(this))
	{
		// if ProcessEvent did not process either a EnterState or a Stop (this might happen if the execution flow has stopped on a latent function)
		// we still need to replicate the fact that we exited the state
		check(!activeNodeGuid.IsValid());
		replicationComponent->MulticastUpdateFSM(GetPathName(), exitEventToReplicate, activeNodeGuid); // yes, activeNodeGuid is null, that's ok
		exitEventToReplicate = NAME_None;
	}
}

void UGCFSM::Tick(float rootTime, float deltaTime)
{
	if (activeState && IsValid(activeState))
	{
		while (eventQueue.Num() > 0)
		{
			auto event = eventQueue[0];
			eventQueue.RemoveAt(0);

			if (rootTime <= event.expiryTimeStamp)
			{
				int index = processedEvents.Find(event.name);
				if (index >= numTransitions)
				{
					GC_VLOG_FSM(Verbose, TEXT("event %s is deferred"), *event.name.ToString());
					eventsBeingDeferred.Add(event);
				}
				else if (index >= 0)
				{
					// A transition event
					GC_VLOG_FSM(Verbose, TEXT("event %s executes a transition"), *event.name.ToString());
					ProcessTransition(event.name);

					// if we didn't reach a new state, return immediately, otherwise proceed with ticking the new state
					if (activeState == nullptr)
					{
						return;
					}
				}
				else if (auto handler = (event.triggerInternalEvents ? activeState->GetInternalEventHandler(event.name) : nullptr))
				{
					GC_VLOG_FSM(Verbose, TEXT("event %s executes an internal event handler"), *event.name.ToString());
					float age = rootTime - event.timeStamp;
					activeState->ProcessEvent(handler, &age);
				}
				else
				{
					GC_VLOG_FSM(VeryVerbose, TEXT("event %s is ignored"), *event.name.ToString());
				}
			}
		}

		switch (activeState->PrivateTick(deltaTime))
		{
		case UGCFSMBasicState::TickResult::ProcessTickEvent:
			latentExecutor.ProcessTick(deltaTime);
			break;

		case UGCFSMBasicState::TickResult::ProcessTimeOutEvent:
			GC_VLOG_FSM(Verbose, TEXT("timeout executes a transition"));
			ProcessTransition(UGCFSMBasicState::GetTimeOutEventName());
			break;
		}
	}
}

void UGCFSM::TriggerEvent(const FGCFSMTriggeredEvent& event)
{
	if (role == EGCFSMRole::Proxy)
	{
		// ignore TriggerEvent on proxies
		return;
	}

	bool addEvent = true;
	int discardedEvents = 0;

	switch (event.queuePolicy)
	{
	case EGCFSMTriggerEventQueuePolicy::JustQueue:
		break;

	case EGCFSMTriggerEventQueuePolicy::QueueUnlessAlreadyInQueue:
		{
			float now = GetContainerState()->GetRootTime();
			auto matchesNameExpired = [&] (const FGCFSMTriggeredEvent& e)
			{
				if (event.name == e.name)
				{
					// found an event with same name, check timestamp
					if (now < e.expiryTimeStamp)
					{
						// the event is not expired yet, ignore incoming event
						addEvent = false;
					}
					else
					{
						// this event has expired, remove it from queue
						return true;
					}
				}
				return false;
			};
			// notice that the matchesNameExpired lambda has a side effect of updating addEvent
			discardedEvents = eventQueue.RemoveAll(matchesNameExpired)
							+ eventsBeingDeferred.RemoveAll(matchesNameExpired);
		}
		break;

	case EGCFSMTriggerEventQueuePolicy::DiscardQueuedEventsThenQueue:
		{
			auto matchesName = [&] (const FGCFSMTriggeredEvent& e)
			{
				return event.name == e.name;
			};
			discardedEvents = eventQueue.RemoveAll(matchesName)
							+ eventsBeingDeferred.RemoveAll(matchesName);
		}
		break;
	}

	if (addEvent)
	{
		GC_VLOG_FSM(VeryVerbose, TEXT("event %s queued (%d matching events discarded from queue)"), *event.name.ToString(), discardedEvents);
		eventQueue.Add(event);
	}
	else
	{
		GC_VLOG_FSM(VeryVerbose, TEXT("event %s discarded (also matching %d events discarded from queue)"), *event.name.ToString(), discardedEvents);
	}

	if (event.propagateToSubstates && activeState)
	{
		activeState->TriggerEvent(event);
	}
}

bool UGCFSM::InvokeEntryPoint(UObject* blueprintContext, FName fsmName, UGCFSMState* containerState, EGCFSMReplicationOptions replicationOptions)
{
	check(blueprintContext);
	check(containerState);
	FString fullName { fsmNamespacePrefix };
	fsmName.AppendString(fullName);
	if (auto entryPoint = blueprintContext->FindFunction(*fullName))
	{
		struct
		{
			UGCFSMState* state;
			EGCFSMReplicationOptions replicationOptions;
			UGCFSM* fsm; // this argument is just a placeholder to let start nodes have an "fsm" pin, the value is not actually used
		} params = { containerState, replicationOptions };
		blueprintContext->ProcessEvent(entryPoint, &params);
		return true;
	}
	else
	{
		return false;
	}
}

void UGCFSM::ExitActiveState(FName event)
{
	if (activeState)
	{
		activeState->PrivateExit(event);
		GC_VLOG_FSM_MSG(Log, TEXT("FSM %s: exits state %s"));
		activeState->MarkAsGarbage();
		activeState = nullptr;
		// old object will be eventually destroyed by GC, since it's no longer referenced
		activeNodeGuid = {};

		// Do not replicate the "exit" here. Since it is probable that we are going to enter a new state immediately
		// by simply replicating the new "enter" we avoid sending two multicast events in the same tick
	}
}

void UGCFSM::Stop()
{
	check(activeState == nullptr);

	auto containerState = GetContainerState();
	GC_VLOG_FSM_MSG(Log, TEXT("FSM %s: stopped"));
	MarkAsGarbage();
	containerState->RemoveFSM(GetFName());

	if (role == EGCFSMRole::Authority)
	{
		replicationComponent->MulticastStopFSM(GetPathName(), exitEventToReplicate);
		exitEventToReplicate = NAME_None;
	}
}

void UGCFSM::ReplicatedEnterState(const FGuid& state)
{
	check(activeState == nullptr);
	replicationLatentExecutor.Process(*state.ToString());
	check(activeNodeGuid == state);
}

void UGCFSM::ReplicatedExitState(FName event)
{
	if (activeState)
	{
		ExitActiveState(event);
		latentExecutor.ProcessEvent(UGCFSMBasicState::GetReplicatedExitName());
	}
}

FString UGCFSM::GetDebugInfo() const
{
	return activeState ? activeState->GetDebugInfo() : FString {};
}

#if ENABLE_VISUAL_LOG
const FString& UGCFSM::GetNameForLogging() const
{
	if (nameForLogging.IsEmpty())
	{
		auto containerState = GetContainerState();
		if (containerState->IsRootState())
		{
			nameForLogging = GetName();
		}
		else
		{
			nameForLogging = FString::Printf(TEXT("%s/%s/%s"), *containerState->GetParentFSM()->GetNameForLogging(), *containerState->GetNameForLogging(), *GetName());
		}
	}
	return nameForLogging;
}
#endif

#if WITH_EDITOR
const FString& UGCFSM::GetBalloonDescription() const
{
	if (balloonDescription.IsEmpty())
	{
		auto containerState = GetContainerState();
		if (containerState->IsRootState())
		{
			static const TCHAR* roles[] = { TEXT("<local>"), TEXT("<authority>"), TEXT("<proxy>") };
			balloonDescription = roles[static_cast<int>(GetRole())];
		}
		else
		{
			auto parentFSM = containerState->GetParentFSM();
			balloonDescription = FString::Printf(TEXT("%s/%s"), *parentFSM->GetBalloonDescription(), *parentFSM->GetName());
		}
	}
	return balloonDescription;
}
#endif

void UGCFSM::MakeSnapshot(UGCFSMSnapshot::Builder& builder)
{
	// only local (non-replicated) FSMs participates to snapshots
	if (GetRole() == EGCFSMRole::Local)
	{
		if (activeState)
		{
			GC_VLOG_FSM_MSG(Verbose, TEXT("FSM %s: making a snapshot while in state %s"));
			builder.AddFSM(GetFName(), GetActiveNodeGuid(), stateHistories);
			activeState->MakeSnapshot(builder);
		}
		else
		{
			GC_VLOG_FSM_MSG(Warning, TEXT("FSM %s could not be snapshot, since no state is active"), *GetNameForLogging());
		}
	}
}

UGCFSMSnapshot* UGCFSM::GetHistoryForActiveState()
{
	TObjectPtr<UGCFSMSnapshot> history;
	if (stateHistories.RemoveAndCopyValue(activeNodeGuid, history))
	{
		GC_VLOG_FSM_MSG(Log, TEXT("FSM %s: restoring history of state %s"));
	}
	return history;
}

void UGCFSM::SaveHistoryForActiveState(UGCFSMState* state)
{
	check(state == activeState);
	if (GetContainerState()->IsRestoringSnapshot())
	{
		GC_VLOG_FSM_MSG(Verbose, TEXT("FSM %s: skips saving history of state %s before exiting it, since we are restoring a snapshot"));
	}
	else
	{
		stateHistories.Add(activeNodeGuid, state->MakeFSMSnapshot());
		GC_VLOG_FSM_MSG(Log, TEXT("FSM %s: saves history of state %s before exiting it"));
	}
}