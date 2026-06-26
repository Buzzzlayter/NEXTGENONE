// Copyright (c) 2018 Gamecentric, All rights reserved


#pragma once

#include "CoreMinimal.h"
#include "GCFSMConstants.h"
#include "GCFSMEntryPointNodeBase.h"
#include "GCFSMInternalEventNode.generated.h"

UCLASS()
class UGCFSMInternalEventNode : public UGCFSMEntryPointNodeBase
{
	GENERATED_BODY()

public:
	//~ Begin UEdGraphNode Interface
	FText GetNodeTitle(ENodeTitleType::Type titleType) const override;
	FText GetTooltipText() const override;
	FSlateIcon GetIconAndTint(FLinearColor& outColor) const override;
	bool IsCompatibleWithGraph(const UEdGraph* targetGraph) const override;
	//~ End UEdGraphNode Interface

protected:
	FText GetMenuTitle() const override;
	FString GetNamespacePrefix() const final { return internalEventNamespacePrefix; }
	FString GetDefaultNodeTitle() const override;
	TTuple<FName, UClass*> GetExternalMember() const override;
};
