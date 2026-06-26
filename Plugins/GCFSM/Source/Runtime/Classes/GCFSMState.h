// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSM.h"
#include "GCFSMBasicState.h"
#include "GCFSMCompatibility.h"
#include "Engine/EngineBaseTypes.h"
#include "GCFSMState.generated.h"

class UGCFSMRootState;

UENUM()
enum class EGCFSMGetActiveStateResult : uint8
{
	State, NoState
};

UENUM(BlueprintType)
enum class EGCFSMTriggerEventTargetPolicy : uint8
{
	// The effective target is the Context object of the target object
	ContextObject,

	// The effective target is the target object
	TargetObject,
};

UENUM()
enum class EGCFSMReplicatedExec : uint8
{
	AuthorityOrLocal, Proxy
};

UENUM(BlueprintType)
enum class EGCFSMReplicationOptions : uint8
{
	// This FSM is executed locally, without replication
	NonReplicated,

	// This FSM is executed on authorities and replicated on all proxies
	Replicated,
};

UCLASS(Blueprintable, HideDropdown, ClassGroup = "GC FSM", DisplayName = "GC FSM State")
class GCFSM_API UGCFSMState : public UGCFSMBasicState
{
	GENERATED_BODY()

public:
	/// Get the context object
	UObject* GetContextObject() const { return contextObject.Get(); }

	/// Get the object whose blueprint has the FSM definitions
	virtual UObject* GetBlueprintContext() { return this; }

	/// Get the root time. This time is used to timestamp all event triggered by this context.
	float GetRootTime() const;

	/// Get the root state object
	UGCFSMRootState* GetRootState() const { return rootState; }

	/// Check if this state is the root state for its context
	bool IsRootState() const;

	/// Check if this state has been properly initialized
	bool IsInitialized() const { return rootState != nullptr; }

	/// Check if the specified FSM currently running
	bool IsFSMRunning(FName fsmName) const;

	// UObject interface
	UWorld* GetWorld() const override { auto obj = contextObject.Get();  return obj ? obj->GetWorld() : nullptr; }
	// End of UObject interface

#if WITH_EDITOR
	/// Used internally by the editor module to provide debugging support
	UGCFSM* GetActiveFSMForGraphNode(const FGuid& nodeGuid) const;
#endif

	/// Returns the "Context" property for the given class, if present
	static FObjectProperty* GetContextProperty(UClass* klass);

	/// Returns the class of the "Context" property for the given class, if present
	static UClass* GetContextClassForClass(UClass* klass);

	// UGCFSMBasicState public interface
	FString GetDebugInfo() const override;
	// End of UGCFSMBasicState public interface

	UGCFSM* GetFSM(FName fsmName) const;
	UGCFSM* GetFSMRecursive(FName name) const;
	UGCFSM* ResolveFSMPathName(const TArray<FName>& fsmPathName, bool startIfNecessary, int startIdx = 0) override;

	// These ought to be private, but we need to make them public to allow Nativization
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UGCFSMState* MakeStateObject(UGCFSM* fsm, TSubclassOf<UGCFSMState> implementationClass, bool processTickEvent, float timeOut, const FString& stateName);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", ExpandEnumAsExecs = "exec"))
	UGCFSM* MakeFSM(FName fsmName, EGCFSMReplicationOptions replicationOptions, EGCFSMReplicatedExec& exec);

	UGCFSMSnapshot* MakeFSMSnapshot();

	void RestoreFSMSnapshot(const UGCFSMSnapshot* snapshot);

	bool IsRestoringSnapshot() const
	{
		return restoringSnapshot;
	}

protected:
	/// C++ function called when entering the state. Base implementation just call OnEnter()
	virtual void Enter() { OnEnter(); }

	/// C++ function called each tick while the state is active. Base implementation just call OnTick()
	virtual void Tick(float deltaTime) { OnTick(deltaTime); }

	/// C++ called when exiting the state, before stopping sub-FSMs, to check if we should save the state history
	virtual bool ShouldSaveHistoryBeforeExiting() const { return false; }

	/// C++ function called when exiting the state, after stopping sub-FSMs. Base implementation just call OnExit()
	virtual void Exit(FName event) { OnExit(event); }

	/// Blueprint event called when entering the state
	UFUNCTION(BlueprintImplementableEvent, Category = "GC FSM")
	void OnEnter();

	/// Blueprint event called each tick while the state is active
	UFUNCTION(BlueprintImplementableEvent, Category = "GC FSM")
	void OnTick(float deltaTime);

	/// Blueprint event called when exiting the state
	UFUNCTION(BlueprintImplementableEvent, Category = "GC FSM")
	void OnExit(FName event);

	/// Blueprint function called to popuplate the debug balloon, while the state is active
	UFUNCTION(BlueprintImplementableEvent, Category = "GC FSM")
	FString GetFSMDebugInfo() const;

	void Initialize(UObject* context, UGCFSMState* parent, bool processTickEvent, float timeOut, const FString& stateName);

#if ENABLE_VISUAL_LOG
	FString MakeDetailedNameForLogging() const override;
#endif

private:
	FWeakObjectPtr contextObject;

	UPROPERTY(Transient)
	TObjectPtr<UGCFSMRootState> rootState;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UGCFSM>> FSMs;

	// placeholder for entry points
	UFUNCTION(meta = (BlueprintInternalUseOnly = "true"))
	void InternalEventEntryPoint(float age);

	void LaunchFSM(UObject* blueprintContext, FName fsmName, EGCFSMReplicationOptions replicationOptions);
	void RemoveFSM(FName fsmName);

	void UpdateReplicatedFSM(const TArray<FName>& fsmPathName, FName exitEvent, const FGuid& stateGuid);
	void StopReplicatedFSM(const TArray<FName>& fsmPathName, FName exitEvent);
	bool IsProxy() const;

	bool restoringSnapshot;

	void MakeSnapshot(UGCFSMSnapshot::Builder& builder) const override;
	void RestoreSnapshot(UGCFSMSnapshot::Iterator& iterator) override;

	// UGCFSMBasicState private interface
	void PrivateEnter() final;
	TickResult PrivateTick(float deltaTime) final;
	void PrivateExit(FName event) final;
	void TriggerEvent(const FGCFSMTriggeredEvent& event) final;
	UFunction* GetInternalEventHandler(FName eventName) final;
	// End of UGCFSMBasicState private interface

	friend class UGCFSM;
	friend class UGCFSMRootState;
	friend class UGCFSMUtilities;
	friend class FGCFSMContext;
	friend class UGCFSMReplicationComponent;
};