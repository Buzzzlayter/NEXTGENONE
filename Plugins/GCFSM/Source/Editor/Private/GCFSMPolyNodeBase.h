// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMBasicNode.h"
#include "GCFSMPolyNodeBase.generated.h"

/// Base class for non-FSM nodes with a Target pin that accept "polymorphically" either a UGCFSMState or anything else
UCLASS(abstract)
class GCFSMEDITOR_API UGCFSMPolyNodeBase : public UGCFSMBasicNode
{
	GENERATED_BODY()

public:
	enum : int
	{
		OutExecPinIndex = UGCFSMBasicNode::NextPinIndex,
		TargetPinIndex,
		NextPinIndex,
	};

	// UEdGraphNode interface
	void AllocateDefaultPins() override;
	// End of UEdGraphNode interface

	// UK2Node interface
	void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	// End of UK2Node interface

	UClass* GetEffectiveTargetClass() const;
};
