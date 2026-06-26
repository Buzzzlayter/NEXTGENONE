// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMPolyNodeWithName.h"
#include "GCFSMIsFSMRunning.generated.h"

UCLASS()
class GCFSMEDITOR_API UGCFSMIsFSMRunning : public UGCFSMPolyNodeWithName
{
	GENERATED_BODY()

public:
	enum : int
	{
		ReturnPinIndex = UGCFSMPolyNodeWithName::NextPinIndex,
		NextPinIndex,
	};

	// UEdGraphNode interface
	void AllocateDefaultPins() override;
	// End of UEdGraphNode interface

	// UEdGraphNode interface
	FSlateIcon GetIconAndTint(FLinearColor& outColor) const override;
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	FText GetTooltipText() const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	void ExpandNode(class FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	// End of UK2Node interface
};
