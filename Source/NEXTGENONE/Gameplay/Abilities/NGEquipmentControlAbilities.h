#pragma once

#include "Abilities/GameplayAbility.h"
#include "NGEquipmentControlAbilities.generated.h"

class UNGCombatComponent;
class UNGEquipmentComponent;

/** Local input lifetime; Combat retains shot requests and the server validates each shot. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGPrimaryAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGPrimaryAttackAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	UFUNCTION() void HandleInputReleased(float TimeHeld);
	bool HasCurrentWeapon() const;
	UFUNCTION() void ContinuePrimaryInput();
	void EquipmentChanged();
	TWeakObjectPtr<UNGCombatComponent> ActiveCombat;
	TWeakObjectPtr<UNGEquipmentComponent> ActiveEquipment;
	FDelegateHandle EquipmentChangedHandle;
	int32 PrimaryRevision = 0;
};

/** Requests item activation to equip, or the existing validated unequip RPC. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGEquipmentControlAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGEquipmentControlAbility();
	UPROPERTY(EditDefaultsOnly, Category="Equipment") bool bEquip = true;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
};
