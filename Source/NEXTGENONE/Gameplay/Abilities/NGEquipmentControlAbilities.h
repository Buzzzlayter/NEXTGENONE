#pragma once

#include "Abilities/GameplayAbility.h"
#include "Gameplay/Combat/NGWeaponTargetData.h"
#include "NGEquipmentControlAbilities.generated.h"

class UNGCombatComponent;
class UNGEquipmentComponent;
class UAbilityTask_WaitGameplayTagRemoved;

/** Predicted input/view transport; only the server schedules shots from the GAS cooldown. */
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
	UFUNCTION() void SendFireView();
	UFUNCTION() void FireCooldownFinished();
	void ReceiveFireView(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void StateTagChanged(FGameplayTag Tag, int32 Count);
	void EquipmentChanged();
	TWeakObjectPtr<UNGCombatComponent> ActiveCombat;
	TWeakObjectPtr<UNGEquipmentComponent> ActiveEquipment;
	FDelegateHandle EquipmentChangedHandle;
	FDelegateHandle ViewDataHandle;
	FDelegateHandle StateTagHandle;
	UPROPERTY() TObjectPtr<UAbilityTask_WaitGameplayTagRemoved> CooldownWait;
	TOptional<FNGPrimaryFireView> ServerView;
	double LastViewReceivedTime = 0.;
	int32 PrimaryRevision = 0;
};

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
