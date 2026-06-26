// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMPauseFSMsNode.h"
#include "GCFSMUtilities.h"
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "GCFSM.PauseFSMsNode"

FText UGCFSMPauseFSMsNode::GetNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Pause FSMs");
}

UFunction* UGCFSMPauseFSMsNode::GetFunction() const
{
	return UGCFSMUtilities::StaticClass()->FindFunctionByName(TEXT("PauseFSMs"));
}

FText UGCFSMPauseFSMsNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip", "Pauses or unpauses all FSMs of the target Context");
}

#undef LOCTEXT_NAMESPACE
