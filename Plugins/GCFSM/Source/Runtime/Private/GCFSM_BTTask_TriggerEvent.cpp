// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSM_BTTask_TriggerEvent.h"
#include "GCFSMModule.h"
#include "GCFSMUtilities.h"
#include "BehaviorTree/BlackboardComponent.h"

UGCFSM_BTTask_TriggerEvent::UGCFSM_BTTask_TriggerEvent(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	NodeName = TEXT("FSM Trigger Event");

	// accept only UObjects as Target and defaults to self
	targetBlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UGCFSM_BTTask_TriggerEvent, targetBlackboardKey), UObject::StaticClass());
	targetBlackboardKey.SelectedKeyName = FBlackboard::KeySelf;

	propagateToSubstates = true;
	triggerInternalEvents = true;
}

void UGCFSM_BTTask_TriggerEvent::InitializeFromAsset(UBehaviorTree& Asset)
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

EBTNodeResult::Type UGCFSM_BTTask_TriggerEvent::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!eventName.IsNone())
	{
		if (auto blackboard = OwnerComp.GetBlackboardComponent())
		{
			if (auto target = blackboard->GetValueAsObject(targetBlackboardKey.SelectedKeyName))
			{
				UGCFSMUtilities::TriggerEvent(target, eventName, targetPolicy, propagateToSubstates, triggerInternalEvents, queuePolicy, expireAfter);
				return EBTNodeResult::Succeeded;
			}
		}
	}
	return EBTNodeResult::Failed;
}

FString UGCFSM_BTTask_TriggerEvent::GetStaticDescription() const
{
	FString desc = FString::Printf(TEXT("Trigger Event: %s"), *eventName.ToString());

	switch (targetPolicy)
	{
	case EGCFSMTriggerEventTargetPolicy::ContextObject:
		desc += FString::Printf(TEXT("\nTarget: (Context of) %s"), *targetBlackboardKey.SelectedKeyName.ToString());
		break;

	case EGCFSMTriggerEventTargetPolicy::TargetObject:
		desc += FString::Printf(TEXT("\nTarget: %s"), *targetBlackboardKey.SelectedKeyName.ToString());
		break;
	}

	if (!propagateToSubstates)
	{
		desc += TEXT("\nDoes NOT trigger in substates");
	}

	if (!triggerInternalEvents)
	{
		desc += TEXT("\nDoes NOT trigger internal events");
	}

	if (queuePolicy != EGCFSMTriggerEventQueuePolicy::JustQueue)
	{
		desc += (queuePolicy == EGCFSMTriggerEventQueuePolicy::QueueUnlessAlreadyInQueue) ? TEXT("\nUnique event: Keep First") : TEXT("\nUnique event: Keep Last");
	}

	if (expireAfter > 0)
	{
		desc += FString::Printf(TEXT("\nEvent expires after: %gs"), expireAfter);
	}

	return desc;
}

#if WITH_EDITOR

FName UGCFSM_BTTask_TriggerEvent::GetNodeIconName() const
{
	static FName name { "GCFSM.BTTaskIcon.TriggerEvent" };
	return name;
}

#endif	// WITH_EDITOR
