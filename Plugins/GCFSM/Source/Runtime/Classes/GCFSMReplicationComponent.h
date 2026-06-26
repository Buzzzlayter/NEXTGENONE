// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "Components/ActorComponent.h"
#include "GCFSMReplicationComponent.generated.h"

enum class EGCFSMTriggerEventQueuePolicy : uint8;

USTRUCT()
struct FNamedGuid
{
	GENERATED_BODY()

	FNamedGuid()
	{}

	FNamedGuid(const TArray<FName>& p, const FGuid& g) : pathName(p), guid(g)
	{}

	UPROPERTY()
	TArray<FName> pathName;

	UPROPERTY()
	FGuid guid;
};

UCLASS(DisplayName = "GC FSM Replication Component", ClassGroup = "GC_FSM", HideCategories = (Collision, ComponentReplication, Activation, Cooking), meta = (BlueprintSpawnableComponent))
class GCFSM_API UGCFSMReplicationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGCFSMReplicationComponent(const FObjectInitializer& objectInitializer);

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastUpdateFSM(const TArray<FName>& fsmPathName, FName exitEvent, const FGuid& activeStateGuid);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopFSM(const TArray<FName>& fsmPathName, FName exitEvent);

	UFUNCTION(BlueprintCallable, Category = "GC FSM", Meta = (Tooltip = "Finalize the initialization of proxy FSMs, required if deferProxyInitialization is set to true. Does nothing if called on authorities, or if initialization already occurred."))
	void FinalizeProxyInitialization();

private:
	UPROPERTY(EditAnywhere, Category = "FSM Replication", Meta = (Tooltip = "If set, the initialization of proxy FSMs is not performed automatically, but only when the FinalizeProxyInitialization method is called explicitly (typically in the actor's BeginPlay). This ensures that the actor is fully initialized before proxy FSMs start."))
	bool deferProxyInitialization;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_ActiveFSMs)
	TArray<FNamedGuid> activeFSMs;

	UFUNCTION()
	void OnRep_ActiveFSMs();
};
