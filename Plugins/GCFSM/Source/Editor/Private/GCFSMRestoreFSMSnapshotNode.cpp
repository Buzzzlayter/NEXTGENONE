// Copyright (c) 2019 Gamecentric, All rights reserved

#include "GCFSMRestoreFSMSnapshotNode.h"
#include "GCFSMUtilities.h"
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "GCFSM.RestoreFSMSnapshotNode"

FText UGCFSMRestoreFSMSnapshotNode::GetNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Restore FSM Snapshot");
}

UFunction* UGCFSMRestoreFSMSnapshotNode::GetFunction() const
{
	return UGCFSMUtilities::StaticClass()->FindFunctionByName(TEXT("RestoreFSMSnapshot"));
}

FText UGCFSMRestoreFSMSnapshotNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip",
		"Restore FSM Snapshot\n\n"
		"Restore the state of the FSMs stored in the specified snapshot object.\n"
		"All such FSMs are launched if necessary and the stored state is entered and made active.\n"
		"FSMs that are not stored in the snapshot (because they were not running, they were replicated\n"
		"or they did not have an active state when the snapshot was created), are not affected.");
}

#undef LOCTEXT_NAMESPACE
