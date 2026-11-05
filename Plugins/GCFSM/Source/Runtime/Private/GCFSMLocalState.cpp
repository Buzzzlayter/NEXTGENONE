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

void UGCFSMLocalState::Initialize(const FString& InGraphName, UObject* InBlueprintContext, bool bInProcessTickEvent, float InTimeOut, const FString& InStateName)
{
	check(InBlueprintContext);
	this->graphName = InGraphName;
	this->blueprintContext = InBlueprintContext;
	entryFunction = InBlueprintContext->FindFunction(*FString::Printf(TEXT("GCFSM_%s_OnEntry"), *InGraphName));
	tickFunction = InBlueprintContext->FindFunction(*FString::Printf(TEXT("GCFSM_%s_OnTick"), *InGraphName));
	exitFunction = InBlueprintContext->FindFunction(*FString::Printf(TEXT("GCFSM_%s_OnExit"), *InGraphName));
	UGCFSMBasicState::Initialize(bInProcessTickEvent, InTimeOut, InStateName);
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
