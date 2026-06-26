// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMPolyNodeBase.h"
#include "GCFSMPolyNodeWithName.generated.h"

class UGCFSMStartNode;

/// Extends UGCFSMPolyNodeBase class for non-FSM nodes, adding a combobox for selecting an FSM by name
UCLASS(abstract)
class GCFSMEDITOR_API UGCFSMPolyNodeWithName : public UGCFSMPolyNodeBase
{
	GENERATED_BODY()

public:
	enum : int
	{
		NamePinIndex = UGCFSMPolyNodeBase::NextPinIndex,
		NextPinIndex,
	};

	// UEdGraphNode interface
	void AllocateDefaultPins() override;
	void PostPlacedNewNode() override;
	void ReconstructNode() override;
	void PinDefaultValueChanged(UEdGraphPin* pin) override;
	void NodeConnectionListChanged() override;
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
	bool HasExternalDependencies(TArray<UStruct*>* optionalOutput) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	void ExpandNode(FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	// End of UK2Node interface

private:
	class VisualWidget;

	UPROPERTY()
	FGuid fsmNodeGuid;

	void CheckGuid(bool tryDefault);
	TArray<UGCFSMStartNode*> GetValidFSMNodes();
};
