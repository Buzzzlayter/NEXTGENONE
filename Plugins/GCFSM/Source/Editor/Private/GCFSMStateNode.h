// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMNodeBase.h"
#include "GCFSMBasicState.h"
#include "GCFSMDetailCustomization.h"
#include "GCFSMCompatibility.h"
#include "EdGraph/EdGraph.h"
#include "GCFSMStateNode.generated.h"

class UGCFSMStartNode;

UENUM()
enum class EGCFSMStateNodeKind
{
	Regular, Local, Submachine
};

UCLASS()
class GCFSMEDITOR_API UGCFSMStateNode : public UGCFSMNodeBase, public IGCFSMDetailCustomization
{
	GENERATED_BODY()

public:
	UGCFSMStateNode(const FObjectInitializer& objectInitializer);

	enum : int
	{
		ImplementationPinIndex = UGCFSMNodeBase::NextPinIndex,
		ReplicationOptionPinIndex,
		ReplicatedExitPinIndex,
		TransitionsPinStartIndex,
	};

	UPROPERTY(EditAnywhere, Category = "FSM State", meta = (GCFSM_KeepOrder = "1"))
	TArray<FName> transitionEvents;

	UPROPERTY(EditAnywhere, Category = "FSM State", meta = (ClampMin = "0.0", UIMin = "0.0", GCFSM_SyncWithPin = "TimeOut"))
	float timeOut;

	UPROPERTY(EditAnywhere, Category = "FSM State")
	TArray<FName> deferredEvents;

	UPROPERTY(EditAnywhere, Category = "FSM State")
	bool showTickFunction;

	// UObject interface
	void Serialize(FArchive& ar) override;
	void PostEditChangeProperty(FPropertyChangedEvent& propertyChangedEvent) override;
	// End of UObject interface

	// UEdGraphNode interface
	bool ShouldShowNodeProperties() const override { return true; }
	void AllocateDefaultPins() override;
	void PostPlacedNewNode() override;
	void PostReconstructNode() override;
	FText GetTooltipText() const override;
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
#if ENGINE_MAJOR_VERSION ==	4 && ENGINE_MINOR_VERSION < 24
	void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
#else
	void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
#endif

	TSharedPtr<INameValidatorInterface> MakeNameValidator() const override;
	void OnRenameNode(const FString& newName) override;
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	FSlateIcon GetIconAndTint(FLinearColor& outColor) const override;
	UObject* GetJumpTargetForDoubleClick() const override;
	bool HasExternalDependencies(TArray<class UStruct*>* optionalOutput) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& oldPins) override;
	void PinDefaultValueChanged(UEdGraphPin* pin) override;
	void PinConnectionListChanged(UEdGraphPin* pin) override;
	void NodeConnectionListChanged() override;
	FText GetToolTipHeading() const override;
	void ValidateNodeDuringCompilation(class FCompilerResultsLog& messageLog) const override;
	void ExpandNode(FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	FName GetCornerIcon() const override;
	bool NodeCausesStructuralBlueprintChange() const override { return true; }
	ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* newPin, int32 newPinIndex, const UEdGraphPin* oldPin, int32 oldPinIndex) const override;
	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface

	UEdGraphPin* GetReplicatedExitPin() const
	{
		return replicatedExitPin ? replicatedExitPin : Pins[ReplicatedExitPinIndex];
	}

	static FName GetTimeOutEventPinName()
	{
		return UGCFSMBasicState::GetTimeOutEventName();
	}

	static FName GetTickPinName()
	{
		return UGCFSMBasicState::GetTickEventName();
	}

	// In Unreal 4.19 the PinName type changed from FString to FName, these function are provided to guarantee backward compatibility

#if ENGINE_MAJOR_VERSION ==	4 && ENGINE_MINOR_VERSION < 19
	static bool IsTimeOutEventPin(const UEdGraphPin* pin)
	{
		static FString name { GetTimeOutEventPinName().ToString() };
		return pin->PinName == name;
	}

	static bool IsTickPin(const UEdGraphPin* pin)
	{
		static FString name { GetTickPinName().ToString() };
		return pin->PinName == name;
	}

	static bool IsReplicatedExitPin(const UEdGraphPin* pin)
	{
		static FString name { UGCFSMBasicState::GetReplicatedExitName().ToString() };
		return pin->PinName == name;
	}
#else
	static bool IsTimeOutEventPin(const UEdGraphPin* pin)
	{
		return pin->PinName == GetTimeOutEventPinName();
	}

	static bool IsTickPin(const UEdGraphPin* pin)
	{
		return pin->PinName == GetTickPinName();
	}

	static bool IsReplicatedExitPin(const UEdGraphPin* pin)
	{
		return pin->PinName == UGCFSMBasicState::GetReplicatedExitName();
	}
#endif

protected:
	// IGCFSMDetailCustomization interface
	// End of IGCFSMDetailCustomization interface

private:
	UPROPERTY()
	FString NodeName;

	UPROPERTY(meta = (GCFSM_VisiblePins = "1"))
	uint32 visiblePins;

	UPROPERTY()
	bool localState_DEPRECATED;

	UPROPERTY()
	EGCFSMStateNodeKind nodeKind;

	bool IsRegularState() const		{ return nodeKind == EGCFSMStateNodeKind::Regular; }
	bool IsLocalState() const		{ return nodeKind == EGCFSMStateNodeKind::Local; }
	bool IsSubmachineState() const	{ return nodeKind == EGCFSMStateNodeKind::Submachine; }

	UPROPERTY()
	FGraphReference localImplementationGraph;

	UPROPERTY()
	FGuid submachineNodeGuid;

	int indexOfEventPinBeingRenamed = -1;
	bool bReconstructNode;

	void AddTransitionEvent(int eventIndex);
	void RemoveTransitionEvent(int eventIndex);
	bool HasImplementationClass() const;
	UEdGraphPin* GetTimeOutEventPin() const;
	UEdGraphPin* GetTimeOutPin() const;
	UEdGraphPin* GetTickPin() const;
	UEdGraphPin* GetDeltaTimePin() const;
	FText GetDefaultTitle() const;
	FString SuggestedNodeName() const;
	void SetTimedEvenPinFriendlyName();

	UEdGraphPin* replicatedExitPin;

	struct ContextCheckResult
	{
		enum
		{
			Compatible,
			Incompatible,
			Error,
		} result;
		UClass* contextClass;
		UClass* implementationClass;
		UClass* implementationContextClass;
	};

	UClass* GetImplementationClass() const;
	ContextCheckResult CheckContext() const;
	void PrepareJumpTarget();
	void CreateNewImplementation(UClass* contextClass) const;
	void CreateNewLocalImplementation();
	void CreateNewSubmachineImplementation();
	void AddContextVariable(UClass* contextClass, bool modifyExisting) const;
	void CreatePinsForClass(UClass* implementationClass, TArray<UEdGraphPin*>* classPins = nullptr);
	void SetNodeKind(EGCFSMStateNodeKind newKind);
	UGCFSMStartNode* GetSubmachineNode() const;

	class VisualWidget;
};

