// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMReplicationComponent.h"
#include "GCFSMUtilities.h"
#include "GCFSMState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UGCFSMReplicationComponent::UGCFSMReplicationComponent(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	SetIsReplicatedByDefault(true);
	bAutoActivate = true;
}

void UGCFSMReplicationComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UGCFSMReplicationComponent, activeFSMs, COND_InitialOnly);
}

void UGCFSMReplicationComponent::OnRep_ActiveFSMs()
{
	AActor* actor = GetOwner();
	check(!actor->HasAuthority());

	if (!deferProxyInitialization)
	{
		FinalizeProxyInitialization();
	}
}

void UGCFSMReplicationComponent::FinalizeProxyInitialization()
{
	AActor* actor = GetOwner();
	if (!actor->HasAuthority())
	{
		auto* rootState = UGCFSMUtilities::GetOrMakeRootStateFromContext(GetOwner());
		for (auto& fsm : activeFSMs)
		{
			rootState->UpdateReplicatedFSM(fsm.pathName, NAME_None, fsm.guid);
		}
		// Proxy initialization is complete, we don't need this data any longer, since the rest
		// of the communication with the server passes through RPCs MulticastUpdateFSM and MulticastStopFSM
		activeFSMs.Empty();
		deferProxyInitialization = false;
	}
}

void UGCFSMReplicationComponent::MulticastUpdateFSM_Implementation(const TArray<FName>& fsmPathName, FName exitEvent, const FGuid& activeStateGuid)
{
	AActor* actor = GetOwner();
	if (actor->HasAuthority() || deferProxyInitialization)
	{
		if (auto fsm = activeFSMs.FindByPredicate([&](const FNamedGuid& ng) { return ng.pathName == fsmPathName; }))
		{
			fsm->guid = activeStateGuid;
		}
		else
		{
			activeFSMs.Emplace(fsmPathName, activeStateGuid);
		}
	}
	else
	{
		auto rootState = UGCFSMUtilities::GetOrMakeRootStateFromContext(GetOwner());
		rootState->UpdateReplicatedFSM(fsmPathName, exitEvent, activeStateGuid);
	}
}

void UGCFSMReplicationComponent::MulticastStopFSM_Implementation(const TArray<FName>& fsmPathName, FName exitEvent)
{
	AActor* actor = GetOwner();
	if (actor->HasAuthority() || deferProxyInitialization)
	{
		activeFSMs.RemoveAll([&](const FNamedGuid& ng) { return ng.pathName == fsmPathName; });
	}
	else if (auto rootState = UGCFSMUtilities::GetRootStateFromContext(GetOwner()))
	{
		rootState->StopReplicatedFSM(fsmPathName, exitEvent);
	}
}
