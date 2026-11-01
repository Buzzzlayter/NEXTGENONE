// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#include "NGInventoryItemDefinition.h"

#include "Templates/SubclassOf.h"
#include "UObject/ObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NGInventoryItemDefinition)

//////////////////////////////////////////////////////////////////////
// UNGInventoryItemDefinition

UNGInventoryItemDefinition::UNGInventoryItemDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const UNGInventoryItemFragment* UNGInventoryItemDefinition::FindFragmentByClass(TSubclassOf<UNGInventoryItemFragment> FragmentClass) const
{
	if (FragmentClass != nullptr)
	{
		for (UNGInventoryItemFragment* Fragment : Fragments)
		{
			if (Fragment && Fragment->IsA(FragmentClass))
			{
				return Fragment;
			}
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////
// UNGInventoryItemDefinition

const UNGInventoryItemFragment* UNGInventoryFunctionLibrary::FindItemDefinitionFragment(TSubclassOf<UNGInventoryItemDefinition> ItemDef, TSubclassOf<UNGInventoryItemFragment> FragmentClass)
{
	if ((ItemDef != nullptr) && (FragmentClass != nullptr))
	{
		return GetDefault<UNGInventoryItemDefinition>(ItemDef)->FindFragmentByClass(FragmentClass);
	}
	return nullptr;
}

