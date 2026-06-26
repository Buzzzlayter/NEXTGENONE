// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMAreFSMsPausedNode.h"
#include "GCFSMUtilities.h"
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "GCFSM.AreFSMsPausedNode"

FText UGCFSMAreFSMsPausedNode::GetNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Are FSMs Paused");
}

UFunction* UGCFSMAreFSMsPausedNode::GetFunction() const
{
	return UGCFSMUtilities::StaticClass()->FindFunctionByName(TEXT("AreFSMsPaused"));
}

FText UGCFSMAreFSMsPausedNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip", "Returns true if the FSMs of the target Context have been paused or false otherwise");
}

#undef LOCTEXT_NAMESPACE
