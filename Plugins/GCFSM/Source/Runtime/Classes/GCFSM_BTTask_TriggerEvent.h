// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GCFSMState.h"
#include "GCFSM_BTTask_TriggerEvent.generated.h"

enum class EGCFSMTriggerEventTargetPolicy : uint8;
enum class EGCFSMTriggerEventQueuePolicy : uint8;

/**
 * GCFSM Trigger an FSM Event.
 * Trigger an FSM Event in the specified Context.
 */
UCLASS(DisplayName = "GC FSM Trigger Event")
class GCFSM_API UGCFSM_BTTask_TriggerEvent : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies")
	FName eventName;

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies", meta = (DisplayName = "Effective Target"))
	EGCFSMTriggerEventTargetPolicy targetPolicy;

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies", meta = (DisplayName = "Trigger in Substates"))
	bool propagateToSubstates;

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies")
	bool triggerInternalEvents;

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies")
	EGCFSMTriggerEventQueuePolicy queuePolicy;

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float expireAfter;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector targetBlackboardKey;

	//~ Begin UBTTaskNode Interface
	void InitializeFromAsset(UBehaviorTree& Asset) override;
	EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	FString GetStaticDescription() const override;

#if WITH_EDITOR
	FName GetNodeIconName() const override;
#endif // WITH_EDITOR

	 //~ End UBTTaskNode Interface
};
