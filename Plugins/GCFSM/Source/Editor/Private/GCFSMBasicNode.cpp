// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMBasicNode.h"
#include "GCFSMEditorModule.h"
#include "KismetCompiler.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"

void UGCFSMBasicNode::AllocateDefaultPins()
{
	// Add exec input pin
	check(Pins.Num() == InExecPinIndex);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, {}, nullptr, UEdGraphSchema_K2::PN_Execute);
}

FLinearColor UGCFSMBasicNode::GetNodeTitleColor() const
{
	return GCFSMNodeColor;
}

FText UGCFSMBasicNode::GetMenuCategory() const
{
	return GCFSMMenuCategory();
}

void UGCFSMBasicNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* klass = GetClass();
	if ((klass->ClassFlags & CLASS_Abstract) == 0)
	{
		if (ActionRegistrar.IsOpenForRegistration(klass))
		{
			auto nodeSpawner = UBlueprintNodeSpawner::Create(klass);
			check(nodeSpawner);
			ActionRegistrar.AddBlueprintAction(klass, nodeSpawner);
		}
	}
}

FNodeHandlingFunctor* UGCFSMBasicNode::CreateNodeHandler(FKismetCompilerContext& compilerContext) const
{
	// returning the default "dummy" handler avoid spurious error messages if ExpandNode fails
	return new FNodeHandlingFunctor(compilerContext);
}
