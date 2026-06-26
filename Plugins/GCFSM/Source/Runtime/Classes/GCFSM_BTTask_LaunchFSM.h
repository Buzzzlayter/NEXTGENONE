// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GCFSMState.h"
#include "GCFSM_BTTask_LaunchFSM.generated.h"

/**
 * GC FSM Launch FSM Event.
 * Launches an FSM on the specified object.
 */
UCLASS(DisplayName = "GC FSM Launch FSM")
class GCFSM_API UGCFSM_BTTask_LaunchFSM : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "Launch FSM")
	FName fsmName;

	UPROPERTY(EditAnywhere, Category = "Launch FSM")
	EGCFSMReplicationOptions replicationOptions;

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
