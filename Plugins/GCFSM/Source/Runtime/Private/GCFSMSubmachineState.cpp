// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMSubmachineState.h"
#include "GCFSMUtilities.h"

UGCFSMSubmachineState* UGCFSMSubmachineState::MakeSubmachineStateObject(UGCFSM* fsm, FName submachineName, EGCFSMSubmachineReplicationOptions replicationOptions, bool processTickEvent, float timeOut, const FString& stateName)
{
	check(fsm);
	auto newState = NewObject<UGCFSMSubmachineState>(fsm);
	newState->Initialize(fsm->GetContainerState(), submachineName, replicationOptions, processTickEvent, timeOut, stateName);
	return newState;
}

void UGCFSMSubmachineState::Initialize(UGCFSMState* parentState, FName submachineName, EGCFSMSubmachineReplicationOptions replicationOptions, bool processTickEvent, float timeOut, const FString& stateName)
{
	check(submachineName.IsValid());
	blueprintContext = parentState->GetBlueprintContext();
	this->submachineName = submachineName;
	this->replicationOptions = replicationOptions;
	UGCFSMState::Initialize(nullptr, parentState, processTickEvent, timeOut, stateName);
}

#if ENABLE_VISUAL_LOG
FString UGCFSMSubmachineState::MakeDetailedNameForLogging() const
{
	return FString::Printf(TEXT("%s, submachine %s"), *GetNameForLogging(), *submachineName.ToString());
}
#endif

void UGCFSMSubmachineState::Enter()
{
	EGCFSMReplicationOptions options;
	UGCFSMSnapshot* history = nullptr;
	switch (replicationOptions)
	{
	default:
	case EGCFSMSubmachineReplicationOptions::SameAsParentFSM:
		options = GetParentFSM()->GetRole() == EGCFSMRole::Local ? EGCFSMReplicationOptions::NonReplicated : EGCFSMReplicationOptions::Replicated;
		break;

	case EGCFSMSubmachineReplicationOptions::NonReplicated:
		options = EGCFSMReplicationOptions::NonReplicated;
		break;

	case EGCFSMSubmachineReplicationOptions::Replicated:
		options = EGCFSMReplicationOptions::Replicated;
		break;

	case EGCFSMSubmachineReplicationOptions::NonReplicatedWithHistory:
		options = EGCFSMReplicationOptions::NonReplicated;
		history = GetParentFSM()->GetHistoryForActiveState();
		break;
	}

	if (history)
	{
		RestoreFSMSnapshot(history);
		history->MarkAsGarbage();
	}
	else
	{
		UGCFSMUtilities::LaunchFSM(this, submachineName, options, blueprintContext);
	}
}