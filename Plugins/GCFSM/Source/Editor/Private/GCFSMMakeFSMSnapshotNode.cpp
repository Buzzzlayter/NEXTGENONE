// Copyright (c) 2019 Gamecentric, All rights reserved

#include "GCFSMMakeFSMSnapshotNode.h"
#include "GCFSMUtilities.h"
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "GCFSM.MakeFSMSnapshotNode"

FText UGCFSMMakeFSMSnapshotNode::GetNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Make FSM Snapshot");
}

UFunction* UGCFSMMakeFSMSnapshotNode::GetFunction() const
{
	return UGCFSMUtilities::StaticClass()->FindFunctionByName(TEXT("MakeFSMSnapshot"));
}

FText UGCFSMMakeFSMSnapshotNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip",
		"Make FSM Snapshot\n\n"
		"Make a snapshot of the state of all the FSMs of the specified context object.\n"
		"The snapshot will save the active state of all FSMs recursively, except those that are not running,\n"
		"that are replicated or that do not have an active state (for example, because they are executing a\n"
		"latent function). Those FSMs will therefore not be affected when the snapshot is restored.");
}

#undef LOCTEXT_NAMESPACE
