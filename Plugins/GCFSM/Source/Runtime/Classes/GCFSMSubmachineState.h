// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSM.h"
#include "GCFSMState.h"
#include "GCFSMSubmachineState.generated.h"

UENUM(BlueprintType)
enum class EGCFSMSubmachineReplicationOptions : uint8
{
	// Submachine FSM is replicate in the same way as its container FSM
	SameAsParentFSM = 0,

	// Submachine FSM is executed locally, without replication
	NonReplicated = 1,

	// Submachine FSM is executed locally and deep history is preserved between invocations
	NonReplicatedWithHistory = 3,

	// Submachine FSM is executed on authorities and replicated on all proxies
	Replicated = 2,
};

UCLASS(DisplayName = "GC FSM Submachine State")
class GCFSM_API UGCFSMSubmachineState : public UGCFSMState
{
	GENERATED_BODY()

public:
	UObject* GetBlueprintContext() override { return blueprintContext; }

	// This ought to be private, but we need to make them public to allow Nativization
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UGCFSMSubmachineState* MakeSubmachineStateObject(UGCFSM* fsm, FName submachineName, EGCFSMSubmachineReplicationOptions replicationOptions, bool processTickEvent, float timeOut, const FString& stateName);

protected:
	void Initialize(UGCFSMState* parentState, FName submachineName, EGCFSMSubmachineReplicationOptions replicationOptions, bool processTickEvent, float timeOut, const FString& stateName);

	UGCFSM* GetSubmachineFSM(FName name) const override { return GetFSMRecursive(name); }

#if ENABLE_VISUAL_LOG
	FString MakeDetailedNameForLogging() const override;
#endif

private:
	UObject* blueprintContext;
	FName submachineName;
	EGCFSMSubmachineReplicationOptions replicationOptions;

	void Enter() final;
	void Tick(float deltaTime) final { }
	bool ShouldSaveHistoryBeforeExiting() const final { return replicationOptions == EGCFSMSubmachineReplicationOptions::NonReplicatedWithHistory; }
	void Exit(FName event) final { }
};
