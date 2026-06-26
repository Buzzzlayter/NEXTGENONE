// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMEntryPointNodeBase.h"
#include "GCFSMEditorModule.h"
#include "GCFSMState.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "FindInBlueprintManager.h"

#define LOCTEXT_NAMESPACE "GCFSM.EntryPointNode"

UGCFSMEntryPointNodeBase::UGCFSMEntryPointNodeBase(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	bOverrideFunction = false;
	bIsEditable = false;
	bCanRenameNode = true;
	bInternalEvent = true;
}

void UGCFSMEntryPointNodeBase::AllocateDefaultPins()
{
	// Set EventReference before calling Super::AllocateDefaultPins, so that Super::AllocateDefaultPins allocates all pins for us
	auto memberData = GetExternalMember();
	if (memberData.Key.IsValid() && memberData.Value != nullptr)
	{
		EventReference.SetExternalMember(memberData.Key, memberData.Value);

		Super::AllocateDefaultPins();

		// Hide the delegate pin, we don't want to user to use it, but we can't remove it since UK2Node_Event needs it
		FindPin(UK2Node_Event::DelegateOutputName)->bHidden = true;
	}
}

FLinearColor UGCFSMEntryPointNodeBase::GetNodeTitleColor() const
{
	return GCFSMNodeColor;
}

void UGCFSMEntryPointNodeBase::OnRenameNode(const FString& newName)
{
	CustomFunctionName = FriendlyNameToFunctionName(*newName);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

class UGCFSMEntryPointNodeBase::NameValidator : public FKismetNameValidator
{
public:
	NameValidator(UGCFSMEntryPointNodeBase const* node)
		: FKismetNameValidator(node->GetBlueprint(), node->CustomFunctionName)
		, blueprint(node->GetBlueprint())
		, namespacePrefix(node->GetNamespacePrefix())
	{}

	// Begin INameValidatorInterface
	EValidatorResult IsValid(FString const& name, bool bOriginal = false) override
	{
		FString functionName = namespacePrefix + name;
		EValidatorResult nameValidity = FKismetNameValidator::IsValid(functionName, bOriginal);
		if ((nameValidity == EValidatorResult::Ok) || (nameValidity == EValidatorResult::ExistingName))
		{
			if (auto ParentFunction = FindUField<UFunction>(blueprint->ParentClass, *functionName))
			{
				nameValidity = EValidatorResult::AlreadyInUse;
			}
		}
		return nameValidity;
	}
	// End INameValidatorInterface

private:
	UBlueprint* blueprint;
	FString namespacePrefix;
};

TSharedPtr<class INameValidatorInterface> UGCFSMEntryPointNodeBase::MakeNameValidator() const
{
	return MakeShareable(new NameValidator(this));
}

void UGCFSMEntryPointNodeBase::ValidateNodeDuringCompilation(class FCompilerResultsLog& messageLog) const
{
	Super::ValidateNodeDuringCompilation(messageLog);

	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint != NULL);

	UFunction* ParentFunction = FindUField<UFunction>(Blueprint->ParentClass, CustomFunctionName);
	if (ParentFunction != NULL)
	{
		auto FuncOwner = ParentFunction->GetOuter();
		check(FuncOwner != NULL);
		messageLog.Error(*FString::Printf(TEXT("@@ name conflicts with a '%s' function"), *FuncOwner->GetName()), this);
	}
}

FText UGCFSMEntryPointNodeBase::GetMenuCategory() const
{
	return GCFSMMenuCategory();
}

void UGCFSMEntryPointNodeBase::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* actionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(actionKey))
	{
		UBlueprintNodeSpawner* nodeSpawner = UBlueprintNodeSpawner::Create(actionKey);
		check(nodeSpawner != nullptr);

		nodeSpawner->DefaultMenuSignature.MenuName = GetMenuTitle();

		FString defaultTitle = GetNamespacePrefix() + GetDefaultNodeTitle();
		nodeSpawner->CustomizeNodeDelegate.BindLambda(
			[defaultTitle](UEdGraphNode* NewNode, bool bIsTemplateNode)
			{
				UGCFSMEntryPointNodeBase* node = CastChecked<UGCFSMEntryPointNodeBase>(NewNode);
				node->CustomFunctionName = FBlueprintEditorUtils::FindUniqueKismetName(node->GetBlueprint(), defaultTitle);
			}
		);

		ActionRegistrar.AddBlueprintAction(actionKey, nodeSpawner);
	}
}

void UGCFSMEntryPointNodeBase::AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddSearchMetaDataInfo(OutTaggedMetaData);

	FString friendlyName = GetFriendlyName();
	for (FSearchTagDataPair& SearchData : OutTaggedMetaData)
	{
		if (SearchData.Key.CompareTo(FFindInBlueprintSearchTags::FiB_Name) == 0)
		{
			SearchData.Value = FText::FromString(FName::NameToDisplayString(friendlyName, false));
			break;
		}
	}
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_NativeName, FText::FromString(friendlyName)));
}

FString UGCFSMEntryPointNodeBase::GetFriendlyName() const
{
	return FunctionNameToFriendlyName(CustomFunctionName);
}

FName UGCFSMEntryPointNodeBase::FriendlyNameToFunctionName(FString friendlyName) const
{
	return *(GetNamespacePrefix() + friendlyName);
}

FString UGCFSMEntryPointNodeBase::FunctionNameToFriendlyName(FName functionName) const
{
	FString out { functionName.ToString() };
	out.RemoveFromStart(GetNamespacePrefix());
	return out;
}

#undef LOCTEXT_NAMESPACE
