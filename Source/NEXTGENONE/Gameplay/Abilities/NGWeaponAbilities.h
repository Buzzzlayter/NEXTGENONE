#pragma once

#include "Gameplay/Abilities/NGGameplayAbilityFromEquipment.h"
#include "NGWeaponAbilities.generated.h"

class UNGCombatComponent;
class UNGInventoryItemInstance;

UCLASS(Blueprintable)
class NEXTGENONE_API UNGReloadAbility : public UNGGameplayAbilityFromEquipment
{
	GENERATED_BODY()
public:
	UNGReloadAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	TWeakObjectPtr<UNGCombatComponent> ReloadCombat;
	TWeakObjectPtr<UNGInventoryItemInstance> ReloadItem;
	UFUNCTION() void FinishReload();
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGFireAbility : public UNGGameplayAbilityFromEquipment
{
	GENERATED_BODY()
public:
	UNGFireAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
};
