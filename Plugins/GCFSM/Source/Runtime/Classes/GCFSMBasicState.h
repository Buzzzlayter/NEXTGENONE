// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSM.h"
#include "Engine/EngineBaseTypes.h"
#include "GCFSMSnapshot.h"
#include "GCFSMBasicState.generated.h"

class UGCFSM;
class UGCFSMState;
struct FGCFSMTriggeredEvent;

UCLASS(DisplayName = "GC FSM Basic State")
class GCFSM_API UGCFSMBasicState : public UObject
{
	GENERATED_BODY()

public:
	/// Returns the time in seconds since this state has been active
	UFUNCTION(BlueprintCallable, Category = "GC FSM")
	float GetActiveTime() const;

	UFUNCTION(BlueprintPure, Category = "GC FSM")
	const FString& GetStateName() const { return stateName; }

	/// Returns a string to display in the debug popup in the editor (overridable)
	virtual FString GetDebugInfo() const { return {}; }

	/// Returns a pointer to FSM that is running this state
	UGCFSM* GetParentFSM() const;

	/// Resolves an FSM pathname, for replication purposes
	virtual UGCFSM* ResolveFSMPathName(const TArray<FName>& fsmPathName, bool startIfNecessary, int startIdx = 0) { return nullptr; }

#if ENABLE_VISUAL_LOG
	const FString& GetNameForLogging() const { return stateName.Len() ? stateName : defaultStateName; }
	const FString& GetDetailedNameForLogging() const;
#else
	FString GetNameForLogging() const { return GetName(); }
#endif

	/// Returns the internal name used for timeout events
	UFUNCTION(BlueprintPure, Category = "GC FSM")
	static FName GetTimeOutEventName()
	{
		static FName name { "GCFSM_Internal_TimeOutEvent" };
		return name;
	}

	/// Returns the internal name used for tick events
	static FName GetTickEventName()
	{
		static FName name { "GCFSM_Internal_Tick" };
		return name;
	}

	static FName GetReplicatedExitName()
	{
		static FName name { "GCFSM_Internal_ReplicatedExit" };
		return name;
	}

	// This ought to be private, but we need to make them public to allow Nativization
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UGCFSMBasicState* MakeBasicStateObject(UGCFSM* fsm, bool processTickEvent, float timeOut, const FString& stateName);

protected:
	void Initialize(bool processTickEvent, float timeOut, const FString& stateName);

#if ENABLE_VISUAL_LOG
	virtual FString MakeDetailedNameForLogging() const;
#endif

	enum class TickResult
	{
		NoProcess, ProcessTickEvent, ProcessTimeOutEvent
	};

	// These functions are not meant to be publicly overridable
	virtual void PrivateEnter() {}
	virtual TickResult PrivateTick(float deltaTime);
	virtual void PrivateExit(FName event) {}
	virtual void TriggerEvent(const FGCFSMTriggeredEvent& event) {}
	virtual UFunction* GetInternalEventHandler(FName eventName) { return nullptr; }

	virtual void MakeSnapshot(UGCFSMSnapshot::Builder& builder) const {}
	virtual void RestoreSnapshot(UGCFSMSnapshot::Iterator& iterator) {}

	virtual UGCFSM* GetSubmachineFSM(FName name) const { return nullptr; }

	friend class UGCFSMState; // to allow unrestricted access to RestoreSnapshot()

private:
	float time;
	float timeOut = FLT_MAX; // by default, a state never times out
	bool processTickEvent;

	FString stateName;

#if ENABLE_VISUAL_LOG
	static const FString defaultStateName;
	mutable FString detailedNameForLogging;
#endif

	friend class UGCFSM;
};

inline float UGCFSMBasicState::GetActiveTime() const
{
	return time;
}
