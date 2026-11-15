#pragma once

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/NetSerialization.h"
#include "NGEquipmentControlAbilities.generated.h"

class UNGCombatComponent;
class UNGEquipmentComponent;
class UAbilityTask_WaitGameplayTagRemoved;

/** View intent for one PrimaryAttack activation; never contains a client hit or damage. */
USTRUCT()
struct NEXTGENONE_API FNGPrimaryFireView : public FGameplayAbilityTargetData
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize Origin = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantizeNormal Direction = FVector::ForwardVector;
	UPROPERTY() int32 EquipmentRevision = INDEX_NONE;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FNGPrimaryFireView> : TStructOpsTypeTraitsBase2<FNGPrimaryFireView>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

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
