// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMLocalStateEntryPointNode.h"
#include "GCFSMUtilities.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "GCFSM.LocalStateEntryPointNode"

UGCFSMLocalStateEntryPointNode::UGCFSMLocalStateEntryPointNode(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	bCanRenameNode = false; // we don't want the user to rename these nodes
}

void UGCFSMLocalStateEntryPointNode::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();

	if (FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num() == 0)
	{
		MakeAutomaticallyPlacedGhostNode();
	}
}

TTuple<FName, UClass*> UGCFSMLocalStateEntryPointNode::GetExternalMember() const
{
	return MakeTuple(FName { GetNodeName(kind) }, UGCFSMState::StaticClass());
}

const TCHAR* UGCFSMLocalStateEntryPointNode::GetNodeName(ELocalStateEntryPoint kind)
{
	static const TCHAR* nodeNames[] { TEXT("OnEntry"), TEXT("OnExit"), TEXT("OnTick") };
	return nodeNames[static_cast<int>(kind)];
}

FName UGCFSMLocalStateEntryPointNode::GetFunctionName(FName graphName, ELocalStateEntryPoint kind)
{
	return *FString::Printf(TEXT("GCFSM_%s_%s"), *graphName.ToString(), GetNodeName(kind));
}

FText UGCFSMLocalStateEntryPointNode::GetNodeTitle(ENodeTitleType::Type titleType) const
{
	if (titleType == ENodeTitleType::FullTitle)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeName"), FText::FromString(GetNodeName(kind)));
		Args.Add(TEXT("GraphName"), FText::FromName(GetGraph()->GetFName()));
		return FText::Format(LOCTEXT("FullTitle", "{NodeName}\nLocal state {GraphName}"), Args);
	}
	else
	{
		return FText::FromString(GetNodeName(kind));
	}
}

FText UGCFSMLocalStateEntryPointNode::GetTooltipText() const
{
	static FText tooltips[] {
		LOCTEXT("ToolTip.OnEntry", "This event is invoked when this local state is entered"),
		LOCTEXT("ToolTip.OnExit", "This event is invoked when this local state is exited"),
		LOCTEXT("ToolTip.OnTick", "This event is invoked every tick while this local state is active")
	};
	return tooltips[static_cast<int>(kind)];
}

void UGCFSMLocalStateEntryPointNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// Don't call Super, we don't want this node to be user-placeable
}

void UGCFSMLocalStateEntryPointNode::ExpandNode(FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph)
{
	if (FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num() > 0)
	{
		if (auto sourceNode = compilerContext.MessageLog.FindSourceObjectTypeChecked<UGCFSMLocalStateEntryPointNode>(this))
		{
			auto realSourceGraph = sourceNode->GetGraph();
			CustomFunctionName = GetFunctionName(realSourceGraph->GetFName(), kind);
		}
	}
	Super::ExpandNode(compilerContext, sourceGraph);
}

#undef LOCTEXT_NAMESPACE
