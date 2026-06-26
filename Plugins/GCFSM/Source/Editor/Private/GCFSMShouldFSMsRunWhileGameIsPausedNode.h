// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMFunctionWrapperNode.h"
#include "GCFSMShouldFSMsRunWhileGameIsPausedNode.generated.h"

UCLASS()
class GCFSMEDITOR_API UGCFSMShouldFSMsRunWhileGameIsPausedNode : public UGCFSMFunctionWrapperNode
{
	GENERATED_BODY()

public:
	// UEdGraphNode interface
	FText GetTooltipText() const override;
	// End of UEdGraphNode interface

protected:
	FText GetNodeTitle() const override;
	UFunction* GetFunction() const override;
};
