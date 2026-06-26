// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSM.h"
#include "GCFSMBasicState.h"
#include "GCFSMLocalState.generated.h"

UCLASS(DisplayName = "GC FSM Local State")
class GCFSM_API UGCFSMLocalState : public UGCFSMBasicState
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GC FSM")
	const FString& GetGraphName() const { return graphName; }

	// This ought to be private, but we need to make them public to allow Nativization
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UGCFSMLocalState* MakeLocalStateObject(UGCFSM* fsm, const FString& localStatePrefix, bool processTickEvent, float timeOut, const FString& stateName);

protected:
	void Initialize(const FString& graphName, UObject* blueprintContext, bool processTickEvent, float timeOut, const FString& stateName);

#if ENABLE_VISUAL_LOG
	FString MakeDetailedNameForLogging() const override;
#endif

	void PrivateEnter() override;
	TickResult PrivateTick(float deltaTime) override;
	void PrivateExit(FName event) override;

private:
	UObject* blueprintContext;
	UFunction* entryFunction;
	UFunction* tickFunction;
	UFunction* exitFunction;
	FString graphName;
};
