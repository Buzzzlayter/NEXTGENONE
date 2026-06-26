// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMPolyNodeBase.h"
#include "GCFSMFunctionWrapperNode.generated.h"

UCLASS(abstract)
class GCFSMEDITOR_API UGCFSMFunctionWrapperNode : public UGCFSMPolyNodeBase
{
	GENERATED_BODY()

public:
	// UEdGraphNode interface
	void AllocateDefaultPins() override;
	FSlateIcon GetIconAndTint(FLinearColor& outColor) const override;
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	void ExpandNode(class FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	// End of UK2Node interface

protected:
	virtual FText GetNodeTitle() const { return {}; }
	virtual UFunction* GetFunction() const { return nullptr; }
	virtual bool TargetIsContext() const { return true; }
};
