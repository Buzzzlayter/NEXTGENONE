// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMUtilities.h"
#include "GCFSMModule.h"
#include "GCFSMRootState.h"
#include "GCFSMSubsystem.h"
#include "UObject/GCObject.h"

UGCFSMState* UGCFSMUtilities::GetRootStateFromContext(UObject* context, bool verbose)
{
	return UGCFSMSubsystem::GetRootStateFromContext(context, verbose);
}

UGCFSMState* UGCFSMUtilities::GetOrMakeRootStateFromContext(UObject* context)
{
	// states can never have root states
	check(context->IsA<UGCFSMBasicState>() == false);
	return UGCFSMSubsystem::GetOrMakeRootStateFromContext(context);
}

/// Get a state object from a stateOrContext
/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
/// @param verbose see same parameter of UGCFSMUtilities::GetRootStateFromContext()
/// @return nullptr if stateOrContext is not a state and there's no root state
UGCFSMState* UGCFSMUtilities::GetState(UObject* stateOrContext, bool verbose)
{
	if (auto state = Cast<UGCFSMState>(stateOrContext))
	{
		check(state->IsInitialized());
		return state;
	}
	else if (auto state = UGCFSMSubsystem::GetRootStateFromContext(stateOrContext, verbose))
	{
		check(state->IsInitialized());
		return state;
	}
	else
	{
		return nullptr;
	}
}

/// Get or make a state object from a stateOrContext
/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
/// @return nullptr only if a root state cannot be created (e.g. if stateOrContext is nullptr or pending kill)
UGCFSMState* UGCFSMUtilities::GetOrMakeState(UObject* stateOrContext)
{
	if (auto state = Cast<UGCFSMState>(stateOrContext))
	{
		check(state->IsInitialized());
		return state;
	}
	else if (auto state = UGCFSMSubsystem::GetOrMakeRootStateFromContext(stateOrContext))
	{
		check(state->IsInitialized());
		return state;
	}
	else
	{
		return nullptr;
	}
}

/// Get the root state object from a stateOrContext
/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
/// @param verbose see same parameter of UGCFSMUtilities::GetRootStateFromContext()
/// @return nullptr if stateOrContext is not a state and there's no root state
UGCFSMRootState* UGCFSMUtilities::GetRootState(UObject* stateOrContext, bool verbose)
{
	if (auto state = Cast<UGCFSMState>(stateOrContext))
	{
		check(state->IsInitialized());
		return state->GetRootState();
	}
	else if (auto state = UGCFSMSubsystem::GetRootStateFromContext(stateOrContext, verbose))
	{
		check(state->IsInitialized());
		return state;
	}
	else
	{
		return nullptr;
	}
}

/// Get or make the root state object from a stateOrContext
/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
/// @return nullptr only if a root state cannot be created (e.g. if stateOrContext is nullptr or pending kill)
UGCFSMRootState* UGCFSMUtilities::GetOrMakeRootState(UObject* stateOrContext)
{
	if (auto state = Cast<UGCFSMState>(stateOrContext))
	{
		check(state->IsInitialized());
		return state->GetRootState();
	}
	else if (auto state = UGCFSMSubsystem::GetOrMakeRootStateFromContext(stateOrContext))
	{
		check(state->IsInitialized());
		return state;
	}
	else
	{
		return nullptr;
	}
}

void UGCFSMUtilities::LaunchFSM(UObject* stateOrContext, FName fsmName, EGCFSMReplicationOptions replicationOptions, UObject* blueprintContext)
{
	if (auto state = GetOrMakeState(stateOrContext))
	{
		state->LaunchFSM(blueprintContext ? blueprintContext : stateOrContext, fsmName, replicationOptions);
	}
	else
	{
		GC_LOG(Warning, TEXT("FSM %s not launched"), *fsmName.ToString());
	}
}

bool UGCFSMUtilities::IsFSMRunning(UObject* stateOrContext, FName fsmName)
{
	if (auto state = GetState(stateOrContext, false))
	{
		return state->IsFSMRunning(fsmName);
	}
	else
	{
		return false;
	}
}

void UGCFSMUtilities::TriggerEvent(UObject* stateOrContext, FName eventName, EGCFSMTriggerEventTargetPolicy targetPolicy, bool propagateToSubstates, bool triggerInternalEvents, EGCFSMTriggerEventQueuePolicy queuePolicy, float expireAfter)
{
	auto state = Cast<UGCFSMState>(stateOrContext);
	if (state == nullptr)
	{
		state = UGCFSMSubsystem::GetRootStateFromContext(stateOrContext);
		if (state == nullptr)
		{
			GC_LOG(Warning, TEXT("Event %s not triggered"), *eventName.ToString());
			return;
		}
	}
	else if (targetPolicy == EGCFSMTriggerEventTargetPolicy::ContextObject)
	{
		state = state->GetRootState();
		check(state)
	}

	check(state->IsInitialized());
	float now = state->GetRootTime();
	float expiryTime = expireAfter > 0 ? now + expireAfter : FLT_MAX;
	state->TriggerEvent({ eventName, now, expiryTime, propagateToSubstates, triggerInternalEvents, queuePolicy });
}

UGCFSMBasicState* UGCFSMUtilities::GetActiveState(UObject* stateOrContext, FName fsmName, TSubclassOf<UGCFSMBasicState> stateClass, EGCFSMGetActiveStateResult& result)
{
	if (auto state = GetState(stateOrContext, true))
	{
		if (auto fsm = state->GetFSMRecursive(fsmName))
		{
			if (UGCFSMBasicState* activeState = fsm->GetActiveState())
			{
				if (activeState->GetClass()->IsChildOf(stateClass))
				{
					result = EGCFSMGetActiveStateResult::State;
					return activeState;
				}
			}
		}
	}

	result = EGCFSMGetActiveStateResult::NoState;
	return nullptr;
}

void UGCFSMUtilities::PauseFSMs(UObject* stateOrContext, bool pause)
{
	if (auto rootState = GetOrMakeRootState(stateOrContext))
	{
		rootState->SetPaused(pause);
	}
}

bool UGCFSMUtilities::AreFSMsPaused(UObject* stateOrContext)
{
	auto rootState = GetRootState(stateOrContext, false);
	return rootState ? rootState->IsPaused() : false;
}

void UGCFSMUtilities::ShouldFSMsRunWhileGameIsPaused(UObject* stateOrContext, bool run)
{
	if (auto rootState = GetOrMakeRootState(stateOrContext))
	{
		rootState->SetTickableWhenGamePaused(run);
	}
}

UGCFSMSnapshot* UGCFSMUtilities::MakeFSMSnapshot(UObject* stateOrContext)
{
	auto state = GetState(stateOrContext, false);
	return state ? state->MakeFSMSnapshot() : nullptr;
}

void UGCFSMUtilities::RestoreFSMSnapshot(UObject* stateOrContext, const UGCFSMSnapshot* snapshot)
{
	if (auto state = GetOrMakeState(stateOrContext))
	{
		state->RestoreFSMSnapshot(snapshot);
	}
}

bool UGCFSMUtilities::IsRestoringFSMSnapshot(UObject* stateOrContext)
{
	auto state = GetState(stateOrContext, false);
	return state ? state->IsRestoringSnapshot() : false;
}