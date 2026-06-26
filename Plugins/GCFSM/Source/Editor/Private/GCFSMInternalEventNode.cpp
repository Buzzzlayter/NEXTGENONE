// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMInternalEventNode.h"
#include "GCFSMEditorModule.h"
#include "GCFSMState.h"
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "GCFSM.InternalEventNode"

TTuple<FName, UClass*> UGCFSMInternalEventNode::GetExternalMember() const
{
	return MakeTuple(FName { "InternalEventEntryPoint" }, UGCFSMState::StaticClass());
}

FText UGCFSMInternalEventNode::GetNodeTitle(ENodeTitleType::Type titleType) const
{
	FText friendlyName = FText::FromString(GetFriendlyName());
	if (titleType != ENodeTitleType::FullTitle)
	{
		return friendlyName;
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("EventName"), friendlyName);
		return FText::Format(LOCTEXT("FullTitle", "{EventName}\nInternal Event Handler"), Args);
	}
}

bool UGCFSMInternalEventNode::IsCompatibleWithGraph(const UEdGraph* targetGraph) const
{
	UBlueprint* blueprint = Cast<UBlueprint>(targetGraph->GetOuter());
	return Super::IsCompatibleWithGraph(targetGraph) && blueprint && blueprint->ParentClass && blueprint->ParentClass->IsChildOf(UGCFSMState::StaticClass());
}

FText UGCFSMInternalEventNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip", "The entry point of an Internal Event handler");
}

FSlateIcon UGCFSMInternalEventNode::GetIconAndTint(FLinearColor& outColor) const
{
	return FSlateIcon("GCFSM", "NodeIcon.InternalEvent");
}

FText UGCFSMInternalEventNode::GetMenuTitle() const
{
	return LOCTEXT("MenuTitle", "Add New Internal Event...");
}

FString UGCFSMInternalEventNode::GetDefaultNodeTitle() const
{
	return LOCTEXT("DefaultTitle", "Event").ToString();
}

#undef LOCTEXT_NAMESPACE
