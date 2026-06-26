// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMState.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include <typeinfo>

namespace GCFSMEditorUtilities
{
	inline UClass* GetContextClassForNode(const UK2Node* node)
	{
		auto bpClass = node->GetBlueprintClassFromNode();
		auto blueprint = Cast<UBlueprint>(bpClass->ClassGeneratedBy);
		// usually we need to use blueprint->GeneratedClass, but, during nativization,
		// blueprint->GeneratedClass is a duplicate of the correct class, which is stored in blueprint->OriginalClass instead
		auto contextClass = blueprint->OriginalClass ? blueprint->OriginalClass.Get() : blueprint->GeneratedClass.Get();
		if (contextClass && contextClass->IsChildOf<UGCFSMState>())
		{
			return UGCFSMState::GetContextClassForClass(bpClass);
		}
		return contextClass;
	}
}
