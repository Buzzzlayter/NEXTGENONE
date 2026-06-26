// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMNodeBase.h"
#include "GCFSMStopNode.generated.h"

UCLASS()
class GCFSMEDITOR_API UGCFSMStopNode : public UGCFSMNodeBase
{
	GENERATED_BODY()

public:
	// UEdGraphNode interface
	FSlateIcon GetIconAndTint(FLinearColor& outColor) const override;
	FText GetTooltipText() const override;
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	void ExpandNode(class FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	// End of UK2Node interface
};
