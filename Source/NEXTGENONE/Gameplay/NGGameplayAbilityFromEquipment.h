// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from Lyra: existing CombatComponent is the equipment grant source.

#pragma once

#include "Abilities/GameplayAbility.h"

#include "NGGameplayAbilityFromEquipment.generated.h"

class UNGCombatComponent;
class UNGInventoryItemInstance;

/**
 * UNGGameplayAbilityFromEquipment
 *
 * An ability granted by and associated with an equipment instance
 */
UCLASS()
class UNGGameplayAbilityFromEquipment : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UNGGameplayAbilityFromEquipment(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category="Lyra|Ability")
	UNGCombatComponent* GetAssociatedEquipment() const;

	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	UNGInventoryItemInstance* GetAssociatedItem() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

};
