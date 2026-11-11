// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbility.h"

#include "NGGameplayAbilityFromEquipment.generated.h"

class UNGEquipmentInstance;
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

    virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
        const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
        FGameplayTagContainer* RelevantTags=nullptr) const override;
    UNGEquipmentInstance* GetEquipmentInstance() const;
	UFUNCTION(BlueprintCallable, Category = "Equipment|Ability")
	UNGInventoryItemInstance* GetAssociatedItem() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

};
