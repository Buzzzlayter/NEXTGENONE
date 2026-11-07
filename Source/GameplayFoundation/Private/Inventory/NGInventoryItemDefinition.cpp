// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryFragment_SetStats.h"
#include "Inventory/NGInventoryFragment_ItemActions.h"
#include "Abilities/GameplayAbility.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

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


bool UNGInventoryItemDefinition::ValidateDefinition(FString& Error) const
{
    if (MaxStackSize < 1) { Error = TEXT("MaxStackSize must be positive."); return false; }
    if (PickupTransform.ContainsNaN()) { Error = TEXT("Pickup transform must be finite."); return false; }
    bool bEquipmentFound = false;
    bool bActionsFound = false;
    TSet<FGameplayTag> StatTags;
    for (const UNGInventoryItemFragment* Fragment : Fragments)
    {
        if (!Fragment) { Error = TEXT("Null item fragment."); return false; }
        if (const auto* Actions = Cast<UNGInventoryFragment_ItemActions>(Fragment))
        {
            if (bActionsFound || !Actions->ActivationAbility)
            { Error = TEXT("Item requires at most one Item Actions fragment with an ActivationAbility."); return false; }
            bActionsFound = true;
            const auto Ability = Actions->ActivationAbility;
            if (Ability->HasAnyClassFlags(CLASS_Abstract)
                || Ability.GetDefaultObject()->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::ServerOnly
                || Ability.GetDefaultObject()->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerActor)
            { Error = TEXT("ActivationAbility requires a concrete ServerOnly/InstancedPerActor ability."); return false; }
        }
        if (const auto* Equipment = Cast<UNGInventoryFragment_Equipment>(Fragment))
        {
            if (bEquipmentFound || !Equipment->Definition) { Error = TEXT("Equipment requires one non-null definition."); return false; }
            bEquipmentFound = true;
        }
        if (const auto* Stats = Cast<UNGInventoryFragment_SetStats>(Fragment))
            for (const auto& Pair : Stats->InitialItemStats)
            {
                if (!Pair.Key.IsValid() || Pair.Value < 0 || StatTags.Contains(Pair.Key))
                { Error = TEXT("Initial stats require valid unique tags and nonnegative counts."); return false; }
                StatTags.Add(Pair.Key);
            }
    }
    Error.Reset(); return true;
}
#if WITH_EDITOR
EDataValidationResult UNGInventoryItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
    const auto Parent = Super::IsDataValid(Context);
    FString Error;
    if (!ValidateDefinition(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
    return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
