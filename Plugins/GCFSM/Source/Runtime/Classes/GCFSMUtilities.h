// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GCFSMState.h"
#include "GCFSMUtilities.generated.h"

class UGCFSMSnapshot;

UCLASS(DisplayName = "GC FSM Utilities")
class GCFSM_API UGCFSMUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/// Internal implementation of the Launch FSM node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @param fsmName the FSM name
	/// @param replicationOptions the replication options for this FSM
	/// @param blueprintContext the object that provides the blueprint function that implements the FSM,
	///		   if blueprintContext == nullptr, the FSM is implemented by stateOrContext
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext"))
	static void LaunchFSM(UObject* stateOrContext, FName fsmName, EGCFSMReplicationOptions replicationOptions, UObject* blueprintContext = nullptr);

	/// Internal implementation of the GetActiveState node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @param fsmName the FSM name
	/// @param stateClass the implementation class for the new state, for example UGCFSMState::StaticClass()
	/// @param result output parameter that holds the result of the internal cast (needed by ExpandEnumAsExecs; not useful in C++, since you can just check the result)
	/// @return a pointer to the current active state in the specified FSM, which is guaranteed to be of the type specified in stateClass, or nullptr in all other cases
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext", DeterminesOutputType = "stateClass", ExpandEnumAsExecs = "result"))
	static UGCFSMBasicState* GetActiveState(UObject* stateOrContext, FName fsmName, TSubclassOf<UGCFSMBasicState> stateClass, EGCFSMGetActiveStateResult& result);

	/// Internal implementation of the TriggerEvent node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @param eventName the event name
	/// @param targetPolicy the target policy (see documentation of the TriggerEvent node)
	/// @param propagateToSubstates true to propagate the event to substates (see documentation of the TriggerEvent node)
	/// @param triggerInternalEvents true to trigger internal event handlers (see documentation of the TriggerEvent node)
	/// @param queuePolicy the queue policy (see documentation of the TriggerEvent node)
	/// @param expireAfter the expiration time in seconds, if greater than 0 (see documentation of the TriggerEvent node)
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext"))
	static void TriggerEvent(UObject* stateOrContext, FName eventName, EGCFSMTriggerEventTargetPolicy targetPolicy = EGCFSMTriggerEventTargetPolicy::ContextObject, bool propagateToSubstates = true, bool triggerInternalEvents = true, EGCFSMTriggerEventQueuePolicy queuePolicy = EGCFSMTriggerEventQueuePolicy::JustQueue, float expireAfter = 0.0f);

	/// Internal implementation of the PauseFSMs node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @param pause true to pause all FSMs of the target context, false to unpause them
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext"))
	static void PauseFSMs(UObject* stateOrContext, bool pause = true);

	/// Internal implementation of the AreFSMsPaused node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @return true if the FSMs of the target context are paused
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext"))
	static bool AreFSMsPaused(UObject* stateOrContext);

	/// Internal implementation of the ShouldFSMsRunWhileGameIsPaused node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @param run true to make the FSMs of the target context run even if the game is paused, false otherwise
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext"))
	static void ShouldFSMsRunWhileGameIsPaused(UObject* stateOrContext, bool run = true);

	/// Internal implementation of the IsFSMRunning node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @param fsmName the FSM name
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext"))
	static bool IsFSMRunning(UObject* stateOrContext, FName fsmName);

	/// Internal implementation of the MakeFSMSnapshot node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @return a snapshot object or nullptr if no root state can be obtained from stateOrContext
	/// @remark only running, non-replicated FSMs are stored into the snapshot object
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext"))
	static UGCFSMSnapshot* MakeFSMSnapshot(UObject* stateOrContext);

	/// Internal implementation of the RestoreFSMSnapshot node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @param snapshot a snapshot object obtained with a call to MakeFSMSnapshot
	/// @remark FSMs that are not stored in the snapshot (because they were not running or they were running as replicated
	/// when MakeFSMSnapshot() was called) are not affected
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext"))
	static void RestoreFSMSnapshot(UObject* stateOrContext, const UGCFSMSnapshot* snapshot);

	/// Internal implementation of the IsRestoringFSMSnapshot node. Can be called from C++ for the same effect.
	/// @param stateOrContext either a state (an instance of UGCFSMState) or a context object (an object that uses FSMs)
	/// @return true if we are in the processing of restoring a snapshot, false otherwise
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "stateOrContext"))
	static bool IsRestoringFSMSnapshot(UObject* stateOrContext);

	/// Get access to the root state, given a context object
	/// This function is not meant for general use, but it can be used in C++ as long as the returned object is not destroyed
	/// The root state is created the first time a context object launches an FSM.
	/// @param context a potential context object
	/// @param verbose if true, emits a log if the context object could be valid, but has no root state attached (i.e.: it has never launched an FSMs)
	/// @return the root state if any, nullptr otherwise
	/// @remark state objects (i.e.: instances of UGCFSMState) are handled specially and though they may launch FSMs, they never have a root state (indeed, they are children of a root state).
	/// This function always returns nullptr for state objects. Use UGCFSMState::GetRootState() to access the root state of an object state.
	static UGCFSMState* GetRootStateFromContext(UObject* context, bool verbose = false);

	/// Get access to the root state, given a context object, creating it if necessary
	/// This function is not meant for general use, but only to implement replication.
	/// @param context a context object
	/// @return the root state if any, nullptr otherwise
	static UGCFSMState* GetOrMakeRootStateFromContext(UObject* context);

private:
	// implementation details
	static UGCFSMState* GetState(UObject* stateOrContext, bool verbose);
	static UGCFSMState* GetOrMakeState(UObject* stateOrContext);
	static UGCFSMRootState* GetRootState(UObject* stateOrContext, bool verbose);
	static UGCFSMRootState* GetOrMakeRootState(UObject* stateOrContext);
};
