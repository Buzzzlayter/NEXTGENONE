// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMShouldFSMsRunWhileGameIsPausedNode.h"
#include "GCFSMUtilities.h"
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "GCFSM.ShouldFSMsRunWhileGameIsPausedNode"

FText UGCFSMShouldFSMsRunWhileGameIsPausedNode::GetNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Should FSMs Run While Game Is Paused");
}

UFunction* UGCFSMShouldFSMsRunWhileGameIsPausedNode::GetFunction() const
{
	return UGCFSMUtilities::StaticClass()->FindFunctionByName(TEXT("ShouldFSMsRunWhileGameIsPaused"));
}

FText UGCFSMShouldFSMsRunWhileGameIsPausedNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip", "Set if the FSMs of the target Context should run even when the game is paused");
}

#undef LOCTEXT_NAMESPACE
