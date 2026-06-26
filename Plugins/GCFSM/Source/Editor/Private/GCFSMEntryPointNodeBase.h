// Copyright (c) 2018 Gamecentric, All rights reserved


#pragma once

#include "CoreMinimal.h"
#include "K2Node_Event.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "GCFSMEntryPointNodeBase.generated.h"

/// Basic class to factor functions common to all GCFSM entry nodes
UCLASS(abstract)
class UGCFSMEntryPointNodeBase : public UK2Node_Event
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UEdGraphNode Interface
	void AllocateDefaultPins() override;
	FLinearColor GetNodeTitleColor() const override;
	void OnRenameNode(const FString& newName) override;
	TSharedPtr<class INameValidatorInterface> MakeNameValidator() const override;
	void AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	void ValidateNodeDuringCompilation(class FCompilerResultsLog& messageLog) const override;
	FText GetMenuCategory() const override;
	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	bool NodeCausesStructuralBlueprintChange() const override { return true; }
	//~ End UK2Node Interface

	FName GetFunctionName() const { return CustomFunctionName; }
	FString GetFriendlyName() const;

	FName FriendlyNameToFunctionName(FString friendlyName) const;
	FString FunctionNameToFriendlyName(FName functionName) const;

protected:
	virtual FText GetMenuTitle() const { return {}; }
	virtual FString GetNamespacePrefix() const { return {}; }
	virtual FString GetDefaultNodeTitle() const { return {}; }
	virtual TTuple<FName, UClass*> GetExternalMember() const { return {}; }

private:
	class NameValidator;
};

template <class T>
inline FName EntryPointFriendlyNameToFunctionName(FString friendlyName)
{
	return GetDefault<T>(T::StaticClass())->FriendlyNameToFunctionName(friendlyName);
}

template <class T>
inline FString EntryPointFunctionNameToFriendlyName(FName functionName)
{
	return GetDefault<T>(T::StaticClass())->FunctionNameToFriendlyName(functionName);
}

template <class T>
T* FindEntryPointNodeByFunctionName(UClass* targetClass, FName functionName)
{
	if (targetClass)
	{
		if (UBlueprint* blueprint = Cast<UBlueprint>(targetClass->ClassGeneratedBy))
		{
			TArray<T*> nodes;
			FBlueprintEditorUtils::GetAllNodesOfClass(blueprint, nodes);
			for (auto node : nodes)
			{
				if (node->GetFunctionName() == functionName)
				{
					return node;
				}
			}
		}
	}
	return nullptr;
}

template <class T>
T* FindEntryPointNodeByFriendlyName(UClass* targetClass, FString friendlyName)
{
	return FindEntryPointNodeByFunctionName<T>(targetClass, EntryPointFriendlyNameToFunctionName<T>(friendlyName));
}

template <class T>
T* FindEntryPointNodeFromFunction(UFunction* function)
{
	if (function)
	{
		if (auto funcOwner = function->GetOuter())
		{
			return FindEntryPointNodeByFunctionName<T>(Cast<UBlueprintGeneratedClass>(funcOwner), function->GetFName());
		}
	}
	return nullptr;
}

