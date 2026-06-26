// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMStopNode.h"
#include "GCFSMEditorModule.h"
#include "GCFSM.h"
#include "KismetCompiler.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"

#define LOCTEXT_NAMESPACE "GCFSM.StopNode"

FSlateIcon UGCFSMStopNode::GetIconAndTint(FLinearColor& outColor) const
{
	return FSlateIcon("GCFSM", "NodeIcon.FSMStop");
}

FText UGCFSMStopNode::GetNodeTitle(ENodeTitleType::Type titleType) const
{
	return LOCTEXT("FullTitle", "FSM Stop");
}

FText UGCFSMStopNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip", "Stops and terminates the current FSM");
}

void UGCFSMStopNode::ExpandNode(FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph)
{
	Super::ExpandNode(compilerContext, sourceGraph);

	if (auto fsmPin = GetFSMPin(compilerContext))
	{
		// Create function node to call StopFsm
		auto funcNode = compilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, sourceGraph);
		funcNode->SetFromFunction(UGCFSM::StaticClass()->FindFunctionByName(TEXT("Stop")));
		funcNode->AllocateDefaultPins();
		compilerContext.MovePinLinksToIntermediate(*Pins[InExecPinIndex], *funcNode->GetExecPin());
		expandedExecPin = funcNode->GetExecPin();
		fsmPin->MakeLinkTo(funcNode->FindPin(UEdGraphSchema_K2::PN_Self));
	}
	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
