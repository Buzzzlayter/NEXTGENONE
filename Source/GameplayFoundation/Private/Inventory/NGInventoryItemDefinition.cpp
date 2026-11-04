// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#include "Inventory/NGInventoryItemDefinition.h"

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

void UNGInventoryItemDefinition::PostLoad()
{
	Super::PostLoad();
	if (!Weapon_DEPRECATED) return;
	const auto* Existing = Cast<UNGInventoryFragment_Equipment>(FindFragmentByClass(UNGInventoryFragment_Equipment::StaticClass()));
	if (!Existing || Existing->Definition != Weapon_DEPRECATED)
	{
		auto* Fragment = NewObject<UNGInventoryFragment_Equipment>(this, NAME_None, RF_Transactional);
		Fragment->Definition = Weapon_DEPRECATED;
		// Child item Blueprints can override the old Weapon value. Never mutate an inherited fragment.
		if (Existing)
		{
			for (auto& Entry : Fragments) if (Entry == Existing) { Entry = Fragment; break; }
		}
		else Fragments.Add(Fragment);
	}
	Weapon_DEPRECATED = nullptr;
}

UDataAsset* UNGInventoryItemDefinition::GetEquipmentDefinition() const
{
	const auto* Fragment = Cast<UNGInventoryFragment_Equipment>(FindFragmentByClass(UNGInventoryFragment_Equipment::StaticClass()));
	return Fragment ? Fragment->Definition.Get() : nullptr;
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

