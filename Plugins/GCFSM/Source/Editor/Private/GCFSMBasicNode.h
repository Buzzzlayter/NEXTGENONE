// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "K2Node.h"
#include "EdGraph/EdGraphPin.h"
#include "GCFSMCompatibility.h"
#include "GCFSMBasicNode.generated.h"

class FKismetCompilerContext;
struct FKismetFunctionContext;

/// Basic class to factor functions common to all GCFSM nodes (except entry nodes, see GCFSMEntryPointNodeBase for those)
UCLASS(Abstract)
class GCFSMEDITOR_API UGCFSMBasicNode : public UK2Node
{
	GENERATED_BODY()

public:
	enum : int
	{
		InExecPinIndex,
		NextPinIndex,
	};

	// UEdGraphNode interface
	void AllocateDefaultPins() override;
	FLinearColor GetNodeTitleColor() const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	FText GetMenuCategory() const override;
	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	FNodeHandlingFunctor* CreateNodeHandler(FKismetCompilerContext& compilerContext) const override;
	// End of UK2Node interface

	// In Unreal 4.19, function UEdGraphNode underwent significant refactoring and PinCategory, PinSubCategory and PinName were changed from FString to FName
	// In order to ease migration to 4.19, Epic introduced several additional overloads for the CreatePin function.
	// Despite the good intentions, those overloads did not help and instead introduced ambiguities. We are therefore declaring here one single CreatePin function
	// (actually, three versions of it for Unreal 4.18) hiding all other declarations provided in UEdGraphNode.

#if ENGINE_MAJOR_VERSION ==	4 && ENGINE_MINOR_VERSION < 19
	// for backward compatibility with Unreal 4.18
	UEdGraphPin* CreatePin(EEdGraphPinDirection Dir, const FString& PinCategory, const FString& PinSubCategory, UObject* PinSubCategoryObject, const char* PinName = nullptr)
	{
		return UEdGraphNode::CreatePin(Dir, PinCategory, PinSubCategory, PinSubCategoryObject, PinName);
	}
	UEdGraphPin* CreatePin(EEdGraphPinDirection Dir, const FString& PinCategory, const FString& PinSubCategory, UObject* PinSubCategoryObject, const FString& PinName)
	{
		return UEdGraphNode::CreatePin(Dir, PinCategory, PinSubCategory, PinSubCategoryObject, PinName);
	}
	UEdGraphPin* CreatePin(EEdGraphPinDirection Dir, const FString& PinCategory, const FString& PinSubCategory, UObject* PinSubCategoryObject, const FName& PinName)
	{
		return UEdGraphNode::CreatePin(Dir, PinCategory, PinSubCategory, PinSubCategoryObject, PinName.ToString());
	}
	static FName GetPinFName(UEdGraphPin* pin)
	{
		return { *pin->GetName() };
	}
#else
	// new version for Unreal 4.19+
	UEdGraphPin* CreatePin(EEdGraphPinDirection Dir, const FName& PinCategory, const FName& PinSubCategory, UObject* PinSubCategoryObject, const FName& PinName = {})
	{
		return UEdGraphNode::CreatePin(Dir, PinCategory, PinSubCategory, PinSubCategoryObject, PinName);
	}
	static FName GetPinFName(UEdGraphPin* pin)
	{
		return pin->GetFName();
	}
#endif
};
