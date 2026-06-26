// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMPolyNodeWithName.h"
#include "GCFSMGetActiveStateNode.generated.h"

class UGCFSMBasicState;

UCLASS()
class GCFSMEDITOR_API UGCFSMGetActiveStateNode : public UGCFSMPolyNodeWithName
{
	GENERATED_BODY()

public:
	enum : int
	{
		StateClassPinIndex = UGCFSMPolyNodeWithName::NextPinIndex,
		OutputPinIndex,
		NoStatePinIndex,
		NextPinIndex,
	};

	// UEdGraphNode interface
	void AllocateDefaultPins() override;
	FSlateIcon GetIconAndTint(FLinearColor& outColor) const override;
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	FText GetTooltipText() const override;
	void PostReconstructNode() override;
	void NodeConnectionListChanged() override;
	void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	// End of UEdGraphNode interface

	// UK2Node interface
	ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	void ExpandNode(FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	// End of UK2Node interface

private:
	void UpdateOutputPinType();
};
