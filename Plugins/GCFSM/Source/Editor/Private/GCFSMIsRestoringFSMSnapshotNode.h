// Copyright (c) 2019 Gamecentric, All rights reserved

#pragma once

#include "GCFSMFunctionWrapperNode.h"
#include "GCFSMIsRestoringFSMSnapshotNode.generated.h"

UCLASS()
class GCFSMEDITOR_API UGCFSMIsRestoringFSMSnapshotNode : public UGCFSMFunctionWrapperNode
{
	GENERATED_BODY()

public:
	// UEdGraphNode interface
	FText GetTooltipText() const override;
	// End of UEdGraphNode interface

protected:
	FText GetNodeTitle() const override;
	UFunction* GetFunction() const override;
	bool TargetIsContext() const override { return false; }
};
