// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GCFSM_BTTask_LaunchFSM.h"
#include "GCFSMModule.h"
#include "GCFSMUtilities.h"
#include "BehaviorTree/BlackboardComponent.h"

UGCFSM_BTTask_LaunchFSM::UGCFSM_BTTask_LaunchFSM(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	NodeName = TEXT("Launch FSM");

	// accept only UObjects as Target and defaults to self
	targetBlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UGCFSM_BTTask_LaunchFSM, targetBlackboardKey), UObject::StaticClass());
	targetBlackboardKey.SelectedKeyName = FBlackboard::KeySelf;
}

void UGCFSM_BTTask_LaunchFSM::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		targetBlackboardKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		UE_LOG(LogBehaviorTree, Warning, TEXT("Can't initialize task: %s, make sure that behavior tree specifies blackboard asset!"), *GetName());
	}
}

EBTNodeResult::Type UGCFSM_BTTask_LaunchFSM::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!fsmName.IsNone())
	{
		if (auto blackboard = OwnerComp.GetBlackboardComponent())
		{
			if (auto target = blackboard->GetValueAsObject(targetBlackboardKey.SelectedKeyName))
			{
				UGCFSMUtilities::LaunchFSM(target, fsmName, replicationOptions);
				return EBTNodeResult::Succeeded;
			}
		}
	}
	return EBTNodeResult::Failed;
}

FString UGCFSM_BTTask_LaunchFSM::GetStaticDescription() const
{
	return FString::Printf(TEXT("Launch FSM '%s' on %s"), *fsmName.ToString(), *targetBlackboardKey.SelectedKeyName.ToString());
}

#if WITH_EDITOR

FName UGCFSM_BTTask_LaunchFSM::GetNodeIconName() const
{
	static FName name { "GCFSM.BTTaskIcon.LaunchFSM" };
	return name;
}

#endif	// WITH_EDITOR
