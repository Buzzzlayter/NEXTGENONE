// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from Lyra: existing CombatComponent is the equipment grant source.

#include "Gameplay/Abilities/NGGameplayAbilityFromEquipment.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Inventory/NGInventoryItemInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(NGGameplayAbilityFromEquipment)

UNGGameplayAbilityFromEquipment::UNGGameplayAbilityFromEquipment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UNGCombatComponent* UNGGameplayAbilityFromEquipment::GetAssociatedEquipment() const
{
	if (FGameplayAbilitySpec* Spec = UGameplayAbility::GetCurrentAbilitySpec())
	{
		return Cast<UNGCombatComponent>(Spec->SourceObject.Get());
	}

	return nullptr;
}

UNGInventoryItemInstance* UNGGameplayAbilityFromEquipment::GetAssociatedItem() const
{
	if (UNGCombatComponent* Equipment = GetAssociatedEquipment())
	{
		return Equipment->GetEquippedItem();
	}
	return nullptr;
}


#if WITH_EDITOR
EDataValidationResult UNGGameplayAbilityFromEquipment::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (InstancingPolicy != EGameplayAbilityInstancingPolicy::InstancedPerActor && InstancingPolicy != EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		Context.AddError(NSLOCTEXT("Lyra", "EquipmentAbilityMustBeInstanced", "Equipment ability must be instanced"));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
