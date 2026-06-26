// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMStartNode.h"
#include "GCFSMEditorModule.h"
#include "GCFSMGraphValidator.h"
#include "GCFSMEditorUtilities.h"
#include "GCFSMReplicationComponent.h"
#include "GCFSMState.h"
#include "GCFSMStateNode.h"
#include "GCFSMStopNode.h"
#include "GCFSMUtilities.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"
#include "K2Node_SwitchName.h"
#include "KismetNodes/SGraphNodeK2Base.h"
#include "KismetNodes/KismetNodeInfoContext.h"
#include "EnumHelpers.h"

#define LOCTEXT_NAMESPACE "GCFSM.StartNode"

TTuple<FName, UClass*> UGCFSMStartNode::GetExternalMember() const
{
	return MakeTuple(FName { "FsmEntryPoint" }, GetClass());
}

void UGCFSMStartNode::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	FindPinChecked(TEXT("containerState"))->bHidden = true;
	FindPinChecked(TEXT("fsm"))->bHidden = true;
	FindPinChecked(TEXT("replicationOptions"))->bHidden = true;
}

FText UGCFSMStartNode::GetNodeTitle(ENodeTitleType::Type titleType) const
{
	FText friendlyName = FText::FromString(GetFriendlyName());
	if (titleType != ENodeTitleType::FullTitle)
	{
		return friendlyName;
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("FSMName"), friendlyName);
		return FText::Format(LOCTEXT("FullTitle", "{FSMName}\nFinite State Machine"), Args);
	}
}

FText UGCFSMStartNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip", "The entry point of a Finite State Machine");
}

FSlateIcon UGCFSMStartNode::GetIconAndTint(FLinearColor& outColor) const
{
	return FSlateIcon("GCFSM", "NodeIcon.FSMStart");
}

FText UGCFSMStartNode::GetMenuTitle() const
{
	return LOCTEXT("MenuTitle", "Add New FSM...");
}

FString UGCFSMStartNode::GetDefaultNodeTitle() const
{
	return LOCTEXT("DefaultTitle", "FSM").ToString();
}

void UGCFSMStartNode::ExpandNode(FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph)
{
	Super::ExpandNode(compilerContext, sourceGraph);

	if (!graphValidator)
	{
		FGraphValidator::ValidateFrom(compilerContext, this);
		check(graphValidator);
	}

	auto makeFsmNode = compilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, sourceGraph);
	makeFsmNode->SetFromFunction(UGCFSMState::StaticClass()->FindFunctionByName("MakeFSM"));
	makeFsmNode->AllocateDefaultPins();
	auto thenPin = FindPinChecked(UEdGraphSchema_K2::PN_Then);
	compilerContext.MovePinLinksToIntermediate(*thenPin, *makeFsmNode->FindPinChecked(TEXT("AuthorityOrLocal")));
	thenPin->MakeLinkTo(makeFsmNode->GetExecPin());
	fsmPin = makeFsmNode->GetReturnValuePin();
	compilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("fsm")), *fsmPin);
	FindPinChecked(TEXT("containerState"))->MakeLinkTo(makeFsmNode->FindPinChecked(UEdGraphSchema_K2::PN_Self));
	FindPinChecked(TEXT("replicationOptions"))->MakeLinkTo(makeFsmNode->FindPinChecked(TEXT("replicationOptions")));
	makeFsmNode->FindPinChecked(TEXT("fsmName"))->DefaultValue = GetFriendlyName();

	// create the intermediate node for replication
	auto replicationWaitNode = compilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, sourceGraph);
	replicationWaitNode->SetFromFunction(UGCFSM::StaticClass()->FindFunctionByName(TEXT("ReplicationWaitFunction")));
	replicationWaitNode->AllocateDefaultPins();
	makeFsmNode->FindPinChecked(TEXT("Proxy"))->MakeLinkTo(replicationWaitNode->GetExecPin());
	fsmPin->MakeLinkTo(replicationWaitNode->FindPinChecked(UEdGraphSchema_K2::PN_Self));

	// create a switch node to implement the jump to a specific node
	// we should have used a switch on FGuid, but we don't have it, so we use a switch on names
	auto switchNode = compilerContext.SpawnIntermediateNode<UK2Node_SwitchName>(this, sourceGraph);

	// add names for all states in this FSM, keep track a stop node, if we find one
	TArray<UGCFSMStateNode*> stateNodes;
	for (auto node : graphValidator->GetFSMNodes())
	{
		if (auto stateNode = Cast<UGCFSMStateNode>(node))
		{
			switchNode->PinNames.Add(*stateNode->NodeGuid.ToString());
			stateNodes.Add(stateNode);
		}
	}

	switchNode->bHasDefaultPin = false; // TODO: should we add a default pin to handle errors?
	switchNode->AllocateDefaultPins();
	replicationWaitNode->GetThenPin()->MakeLinkTo(switchNode->GetExecPin());
	replicationWaitNode->FindPinChecked(TEXT("stateOrStop"))->MakeLinkTo(switchNode->GetSelectionPin());

	auto switchPinPtr = switchNode->Pins.GetData() + 3; // a switch node has 3 pins before the output exec pins

	// connect states
	for (auto stateNode : stateNodes)
	{
		check(stateNode->NodeGuid.ToString() == (*switchPinPtr)->GetName());
		(*switchPinPtr++)->MakeLinkTo(stateNode->GetExpandedExecPin());
		stateNode->GetReplicatedExitPin()->MakeLinkTo(replicationWaitNode->GetExecPin());
	}
}

class UGCFSMStartNode::VisualWidget : public SGraphNodeK2Base
{
	FName fsmName;

public:
	SLATE_BEGIN_ARGS(VisualWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, UK2Node* node)
	{
		GraphNode = node;
		fsmName = *Cast<UGCFSMStartNode>(GraphNode)->GetFriendlyName();
		UpdateGraphNode();
	}

	static UGCFSMState* GetStateFromObject(UObject* object)
	{
		if (auto fsmState = Cast<UGCFSMState>(object))
		{
			return fsmState;
		}
		else if (object)
		{
			return UGCFSMUtilities::GetRootStateFromContext(object, false);
		}
		else
		{
			return nullptr;
		}
	}

	void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override
	{
		SGraphNodeK2Base::GetNodeInfoPopups(Context, Popups);
		FKismetNodeInfoContext* K2Context = (FKismetNodeInfoContext*)Context;
		if (UGCFSMState* fsmState = GetStateFromObject(K2Context->ActiveObjectBeingDebugged))
		{
			if (UGCFSM* fsm = fsmState->GetFSMRecursive(fsmName))
			{
				const FLinearColor orange { 0.9f, 0.5f, 0.0f };
				new (Popups) FGraphInformationPopupInfo(NULL, orange, *fsm->GetBalloonDescription());

				if (fsm->GetRole() == EGCFSMRole::Proxy && !fsm->GetActiveNodeGuid().IsValid())
				{
					const FLinearColor red { 0.9f, 0.0f, 0.0f };
					new (Popups) FGraphInformationPopupInfo(NULL, red, TEXT("Waiting replication data from server"));
				}
			}
		}
	}
};

TSharedPtr<SGraphNode> UGCFSMStartNode::CreateVisualWidget()
{
	return SNew(VisualWidget, this);
}

void UGCFSMStartNode::FsmEntryPoint(UGCFSMState*, EGCFSMReplicationOptions, UGCFSM*)
{
	// This function is never called, we need it simply to have Unreal understand its signature
}

UGCFSMStartNode* UGCFSMStartNode::GetStartNodeFromGuid(UBlueprint* blueprint, const FGuid& nodeGuid)
{
	return Cast<UGCFSMStartNode>(FBlueprintEditorUtils::GetNodeByGUID(blueprint, nodeGuid));
}

#undef LOCTEXT_NAMESPACE
