// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMBasicNode.h"
#include "GCFSMNodeBase.generated.h"

class UGCFSMStartNode;

/// Base class for FSM nodes
UCLASS(Abstract)
class GCFSMEDITOR_API UGCFSMNodeBase : public UGCFSMBasicNode
{
	GENERATED_BODY()

public:
	// UK2Node interface
	bool CanPlaceBreakpoints() const override { return false; }
	// End of UK2Node interface

	UEdGraphPin* GetExpandedExecPin() const
	{
		return expandedExecPin ? expandedExecPin : GetExecPin();
	}

protected:
	TOptional<UGCFSMStartNode*> startNode;
	UEdGraphPin* expandedExecPin;

	UEdGraphPin* GetFSMPin(FKismetCompilerContext& compilerContext);

	friend class FGraphValidator;
};
