// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMBasicState.h"
#include "GCFSM.h"

#if ENABLE_VISUAL_LOG
const FString UGCFSMBasicState::defaultStateName { TEXT("<state>") };
#endif

UGCFSMBasicState* UGCFSMBasicState::MakeBasicStateObject(UGCFSM* fsm, bool processTickEvent, float timeOut, const FString& stateName)
{
	check(fsm);
	auto newState = NewObject<UGCFSMBasicState>(fsm);
	newState->Initialize(processTickEvent, timeOut, stateName);
	return newState;
}

void UGCFSMBasicState::Initialize(bool bInProcessTickEvent, float InTimeOut, const FString& InStateName)
{
	this->processTickEvent = bInProcessTickEvent;
	if (InTimeOut > 0 && GetParentFSM()->GetRole() != EGCFSMRole::Proxy) // Proxies do not honor timeouts
	{
		this->timeOut = InTimeOut;
	}
	this->stateName = InStateName;
}

#if ENABLE_VISUAL_LOG
const FString& UGCFSMBasicState::GetDetailedNameForLogging() const
{
	if (detailedNameForLogging.Len() == 0)
	{
		detailedNameForLogging = MakeDetailedNameForLogging();
	}
	return detailedNameForLogging;
}

FString UGCFSMBasicState::MakeDetailedNameForLogging() const
{
	return stateName.Len() ? stateName : TEXT("<state>");
}
#endif

UGCFSMBasicState::TickResult UGCFSMBasicState::PrivateTick(float deltaTime)
{
	time += deltaTime;

	if (time > timeOut)
	{
		return TickResult::ProcessTimeOutEvent;
	}
	else if (processTickEvent)
	{
		return TickResult::ProcessTickEvent;
	}
	else
	{
		return TickResult::NoProcess;
	}
}

UGCFSM* UGCFSMBasicState::GetParentFSM() const
{
	return Cast<UGCFSM>(GetOuter());
}
