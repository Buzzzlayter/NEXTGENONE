// Copyright (c) 2018 Gamecentric, All rights reserved


#pragma once

#include "CoreMinimal.h"
#include "GCFSMConstants.h"
#include "GCFSMEntryPointNodeBase.h"
#include <memory>
#include "GCFSMStartNode.generated.h"

class UGCFSMState;
class UGCFSMStartNode;
class FGraphValidator;
enum class EGCFSMReplicationOptions : uint8;

UCLASS()
class UGCFSMStartNode : public UGCFSMEntryPointNodeBase
{
	GENERATED_BODY()

public:
	//~ Begin UEdGraphNode Interface
	void AllocateDefaultPins() override;
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	FText GetTooltipText() const override;
	FSlateIcon GetIconAndTint(FLinearColor& outColor) const override;
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	bool ShouldShowNodeProperties() const override { return true; }
	void ExpandNode(FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	//~ End UK2Node Interface

	UEdGraphPin* GetFSMPin() const
	{
		return fsmPin ? fsmPin : FindPinChecked(TEXT("fsm"));
	}

	std::shared_ptr<FGraphValidator> graphValidator;

	static UGCFSMStartNode* GetStartNodeFromGuid(UBlueprint* blueprint, const FGuid& nodeGuid);

protected:
	FText GetMenuTitle() const override;
	FString GetNamespacePrefix() const final { return fsmNamespacePrefix; }
	FString GetDefaultNodeTitle() const override;
	TTuple<FName, UClass*> GetExternalMember() const override;

private:
	UEdGraphPin* fsmPin;

	// placeholder for non-replicated FSM entry points
	UFUNCTION(meta = (BlueprintInternalUseOnly = "true"))
	void FsmEntryPoint(UGCFSMState* containerState, EGCFSMReplicationOptions replicationOptions, UGCFSM* fsm);

	class VisualWidget;
};
