// Copyright (c) 2018 Gamecentric, All rights reserved


#pragma once

#include "CoreMinimal.h"
#include "GCFSMEntryPointNodeBase.h"
#include "GCFSMLocalStateEntryPointNode.generated.h"

UENUM()
enum class ELocalStateEntryPoint : uint8
{
	OnEnter, OnExit, OnTick
};

/// Class that represents any of the three entry points of a local state implementation
UCLASS()
class UGCFSMLocalStateEntryPointNode : public UGCFSMEntryPointNodeBase
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
	ELocalStateEntryPoint kind;

	//~ Begin UEdGraphNode Interface
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	FText GetTooltipText() const override;
	bool CanUserDeleteNode() const override { return false; }
	void NodeConnectionListChanged() override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	void ExpandNode(class FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph) override;
	//~ End UK2Node Interface

	static FName GetFunctionName(FName graphName, ELocalStateEntryPoint kind);

protected:
	TTuple<FName, UClass*> GetExternalMember() const override;

private:
	static const TCHAR* GetNodeName(ELocalStateEntryPoint kind);
};
