// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMPolyNodeWithName.h"
#include "GCFSMDetailCustomization.h"
#include "GCFSMLaunchNode.generated.h"

enum class EGCFSMReplicationOptions : uint8;

UCLASS()
class GCFSMEDITOR_API UGCFSMLaunchNode : public UGCFSMPolyNodeWithName, public IGCFSMDetailCustomization
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "FSM Replication", meta = (GCFSM_SyncWithPin = "replication"))
	EGCFSMReplicationOptions replicationOptions;

	enum : int
	{
		ReplicationOptionsPinIndex = UGCFSMPolyNodeWithName::NextPinIndex,
		NextPinIndex,
	};

	// UObject interface
	void PostEditChangeProperty(FPropertyChangedEvent& propertyChangedEvent) override;
	// End of UObject interface

	// UEdGraphNode interface
	void AllocateDefaultPins() override;
	FSlateIcon GetIconAndTint(FLinearColor& outColor) const override;
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	FText GetTooltipText() const override;
	void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	void PostPlacedNewNode() override;
	void PostReconstructNode() override;
	// End of UEdGraphNode interface

	// UK2Node interface
	FName GetCornerIcon() const override;
	bool ShouldShowNodeProperties() const override { return true; }
	void ExpandNode(class FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	// End of UK2Node interface

private:
	UPROPERTY(meta = (GCFSM_VisiblePins = "1"))
	uint32 visiblePins;
};
