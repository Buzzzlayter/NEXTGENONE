// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMLocalState.h"
#include "GCFSMState.h"
#include "GCFSM.h"

UGCFSMLocalState* UGCFSMLocalState::MakeLocalStateObject(UGCFSM* fsm, const FString& localStatePrefix, bool processTickEvent, float timeOut, const FString& stateName)
{
	check(fsm);
	auto newState = NewObject<UGCFSMLocalState>(fsm);
	auto containerState = fsm->GetContainerState();
	newState->Initialize(localStatePrefix, containerState->GetBlueprintContext(), processTickEvent, timeOut, stateName);
	return newState;
}

void UGCFSMLocalState::Initialize(const FString& graphName, UObject* blueprintContext, bool processTickEvent, float timeOut, const FString& stateName)
{
	check(blueprintContext);
	this->graphName = graphName;
	this->blueprintContext = blueprintContext;
	entryFunction = blueprintContext->FindFunction(*FString::Printf(TEXT("GCFSM_%s_OnEntry"), *graphName));
	tickFunction = blueprintContext->FindFunction(*FString::Printf(TEXT("GCFSM_%s_OnTick"), *graphName));
	exitFunction = blueprintContext->FindFunction(*FString::Printf(TEXT("GCFSM_%s_OnExit"), *graphName));
	UGCFSMBasicState::Initialize(processTickEvent, timeOut, stateName);
}

#if ENABLE_VISUAL_LOG
FString UGCFSMLocalState::MakeDetailedNameForLogging() const
{
	return FString::Printf(TEXT("%s, eventgraph %s"), *GetNameForLogging(), *graphName);
}
#endif

void UGCFSMLocalState::PrivateEnter()
{
	if (entryFunction)
	{
		check(blueprintContext);
		blueprintContext->ProcessEvent(entryFunction, nullptr);
	}
}

UGCFSMLocalState::TickResult UGCFSMLocalState::PrivateTick(float deltaTime)
{
	auto result = Super::PrivateTick(deltaTime);
	if (tickFunction)
	{
		check(blueprintContext);
		blueprintContext->ProcessEvent(tickFunction, &deltaTime);
	}
	return result;
}

void UGCFSMLocalState::PrivateExit(FName event)
{
	if (exitFunction)
	{
		check(blueprintContext);
		blueprintContext->ProcessEvent(exitFunction, &event);
	}
}
