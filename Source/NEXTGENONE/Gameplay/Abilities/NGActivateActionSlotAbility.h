#pragma once

#include "Abilities/GameplayAbility.h"
#include "NGActivateActionSlotAbility.generated.h"

/** Selects an Action slot; the inventory resolves and activates the item's ability on the server. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGActivateActionSlotAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGActivateActionSlotAbility();
	UPROPERTY(EditDefaultsOnly, Category="Inventory", meta=(ClampMin="0")) int32 ActionSlot = 0;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
};
