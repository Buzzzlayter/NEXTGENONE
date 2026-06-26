// Copyright (c) 2019 Gamecentric, All rights reserved

#include "GCFSMIsRestoringFSMSnapshotNode.h"
#include "GCFSMUtilities.h"
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "GCFSM.IsRestoringFSMSnapshotNode"

FText UGCFSMIsRestoringFSMSnapshotNode::GetNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Is Restoring FSM Snapshot");
}

UFunction* UGCFSMIsRestoringFSMSnapshotNode::GetFunction() const
{
	return UGCFSMUtilities::StaticClass()->FindFunctionByName(TEXT("IsRestoringFSMSnapshot"));
}

FText UGCFSMIsRestoringFSMSnapshotNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip",
		"Is Restoring FSM Snapshot\n\n"
		"Returns true if we are in the process of restoring an FSM snapshot for the target state or context");
}

#undef LOCTEXT_NAMESPACE
