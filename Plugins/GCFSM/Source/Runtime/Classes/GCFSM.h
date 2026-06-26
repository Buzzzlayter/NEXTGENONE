// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "Engine/LatentActionManager.h"
#include "VisualLogger/VisualLogger.h"
#include "GCFSMSnapshot.h"
#include "GCFSM.generated.h"

class UGCFSMBasicState;
class UGCFSMState;
class UGCFSMReplicationComponent;
enum class EGCFSMReplicationOptions : uint8;

UENUM(BlueprintType)
enum class EGCFSMTriggerEventQueuePolicy : uint8
{
	// Multiple such events are allowed in the queue.
	// Unconditionally queued this event, regardless of events already in queue.
	JustQueue UMETA(DisplayName = "Multiple events allowed"),

	// Only one such event is allowed in the queue: keep the first one.
	// Queue this event unless there's an unexpired event with this name in the queue. Expired events are removed.
	QueueUnlessAlreadyInQueue UMETA(DisplayName = "Unique event: Keep First"),

	// Only one such event is allowed in the queue: keep the last one.
	// Discard all events with this name already in the queue, then queue this event.
	DiscardQueuedEventsThenQueue UMETA(DisplayName = "Unique event: Keep Last"),
};

struct FGCFSMTriggeredEvent
{
	FName name;
	float timeStamp;
	float expiryTimeStamp;
	bool propagateToSubstates : 1;
	bool triggerInternalEvents : 1;
	EGCFSMTriggerEventQueuePolicy queuePolicy;
};

enum class EGCFSMRole
{
	Local, Authority, Proxy
};

UCLASS(DisplayName = "GC FSM")
class GCFSM_API UGCFSM : public UObject
{
	GENERATED_BODY()

public:
	const FGuid& GetActiveNodeGuid() const { return activeNodeGuid; }
	UGCFSMBasicState* GetActiveState() const { return activeState; }
	float GetActiveStateTime() const;

	UGCFSMState * GetContainerState() const;
	EGCFSMRole GetRole() const { return role; }
	const TArray<FName>& GetPathName() const;

#if WITH_EDITOR
	const TArray<FGCFSMTriggeredEvent>& GetEventsBeingDeferred() const { return eventsBeingDeferred; }
	const FString& GetBalloonDescription() const;
#endif

	FString GetDebugInfo() const;

#if ENABLE_VISUAL_LOG
	const FString& GetNameForLogging() const;
#else
	FString GetNameForLogging() const { return GetName(); }
#endif

	// These ought to be private, but we need to make them public to allow Nativization
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Latent, LatentInfo = "latentInfo"))
	void EnterState(UGCFSMBasicState* state, FGuid nodeGuid, FLatentActionInfo latentInfo, FName& eventName, float& deltaTime, const TArray<FName>& processedEvents, int32 numTransitions);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	void Stop();

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Latent, LatentInfo = "latentInfo"))
	void ReplicationWaitFunction(FName& stateOrStop, FLatentActionInfo latentInfo);

	void MakeSnapshot(UGCFSMSnapshot::Builder& builder);

	UGCFSMSnapshot* GetHistoryForActiveState();
	void SaveHistoryForActiveState(UGCFSMState* state);

private:
	void ExitActiveState(FName event);
	void TriggerEvent(const FGCFSMTriggeredEvent& event);
	void Tick(float rootTime, float deltaTime);
	void ProcessTransition(FName event);

	void SetRole(EGCFSMRole r);
	void ReplicatedEnterState(const FGuid& state);
	void ReplicatedExitState(FName event);

	static bool InvokeEntryPoint(UObject* blueprintContext, FName fsmName, UGCFSMState* containerState, EGCFSMReplicationOptions replicationOptions);

	UPROPERTY(Transient)
	TObjectPtr<UGCFSMBasicState> activeState;

	TArray<FName> processedEvents;
	int numTransitions;

	TArray<FGCFSMTriggeredEvent> eventQueue;
	TArray<FGCFSMTriggeredEvent> eventsBeingDeferred;

	struct LatentExecutor
	{
		UObject* target;
		UFunction* function;
		int32 linkage;
		FName* eventName;
		float* deltaTime;

		void Set(const FLatentActionInfo& latentInfo, FName& eventName, float& deltaTime);
		void ProcessEvent(FName name);
		void ProcessTick(float deltaTime);
	};

	LatentExecutor latentExecutor;
	FGuid activeNodeGuid;

	UPROPERTY(Transient)
	TObjectPtr<UGCFSMReplicationComponent> replicationComponent;

	EGCFSMRole role;
	FName exitEventToReplicate;
	TArray<FName> pathName;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UGCFSMSnapshot>> stateHistories;

	struct ReplicationLatentExecutor
	{
		UObject* target;
		UFunction* function;
		int32 linkage;
		FName* stateOrStop;

		void Set(const FLatentActionInfo& latentInfo, FName& stateOrStop);
		void Process(FName stateOrStop);
	};
	ReplicationLatentExecutor replicationLatentExecutor;

#if ENABLE_VISUAL_LOG
	mutable FString nameForLogging;
#endif

#if WITH_EDITOR
	mutable FString balloonDescription;
#endif

	friend class UGCFSMState;
};
