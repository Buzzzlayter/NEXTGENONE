// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMPolyNodeBase.h"
#include "GCFSMDetailCustomization.h"
#include "GCFSMTriggerEventNode.generated.h"

class UGCFSMStartNode;
enum class EGCFSMTriggerEventTargetPolicy : uint8;
enum class EGCFSMTriggerEventQueuePolicy : uint8;

UCLASS()
class GCFSMEDITOR_API UGCFSMTriggerEventNode : public UGCFSMPolyNodeBase, public IGCFSMDetailCustomization
{
	GENERATED_BODY()

public:
	UGCFSMTriggerEventNode();

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies", meta = (DisplayName = "Effective Target", GCFSM_SyncWithPin = "TargetPolicy"))
	EGCFSMTriggerEventTargetPolicy targetPolicy;

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies", meta = (DisplayName = "Trigger in Substates", GCFSM_SyncWithPin = "PropagateToSubstates"))
	bool propagateToSubstates;

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies", meta = (GCFSM_SyncWithPin = "TriggerInternalEvents"))
	bool triggerInternalEvents;

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies", meta = (GCFSM_SyncWithPin = "QueuePolicy"))
	EGCFSMTriggerEventQueuePolicy queuePolicy;

	UPROPERTY(EditAnywhere, Category = "FSM Trigger Event Policies", meta = (ClampMin = "0.0", UIMin = "0.0", GCFSM_SyncWithPin = "ExpireAfter"))
	float expireAfter;

	enum : int
	{
		NamePinIndex = UGCFSMPolyNodeBase::NextPinIndex,
		TargetPolicyPinIndex,
		PropagatePolicyPinIndex,
		TriggerInternalPolicyPinIndex,
		QueuePolicyPinIndex,
		ExpireAfterPinIndex,
		NextPinIndex,
	};

	// UObject interface
	void PostEditChangeProperty(struct FPropertyChangedEvent& propertyChangedEvent) override;
	// End of UObject interface

	// UEdGraphNode interface
	void AllocateDefaultPins() override;
	FSlateIcon GetIconAndTint(FLinearColor& outColor) const override;
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	FText GetTooltipText() const override;
	void NodeConnectionListChanged() override;
	void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	void PostPlacedNewNode() override;
	void PostReconstructNode() override;
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
	// End of UEdGraphNode interface

	// UK2Node interface
	bool ShouldShowNodeProperties() const override { return true; }
	void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	void ExpandNode(class FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	// End of UK2Node interface

	bool ShouldHideTargetPolicy() const;

protected:
	// IGCFSMDetailCustomization interface
	bool IsPinPropertyAvailable(const PinProperty& pinProperty) const override;
	// End of IGCFSMDetailCustomization interface

private:
	UPROPERTY(meta = (GCFSM_VisiblePins = "1"))
	uint32 visiblePins;

	bool bReconstructNode;

	class VisualWidget;
};
