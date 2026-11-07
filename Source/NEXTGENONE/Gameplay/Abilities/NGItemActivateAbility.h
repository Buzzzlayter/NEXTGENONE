#pragma once

#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "NGItemActivateAbility.generated.h"

class UNGInventoryItemInstance;
class UNGPlayerInventoryManagerComponent;
struct FOnAttributeChangeData;


/** One-shot server grant. SourceObject is the selected item, never the equipped weapon. */
UCLASS(Abstract, Blueprintable)
class NEXTGENONE_API UNGItemActivateAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGItemActivateAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	UFUNCTION(BlueprintPure, Category="Item") UNGInventoryItemInstance* GetAssociatedItem() const;
protected:
	UNGPlayerInventoryManagerComponent* GetInventory() const;
	bool HasCurrentItem() const;
};

UCLASS(DisplayName="Medical Item")
class NEXTGENONE_API UNGInventoryFragment_Medical : public UNGInventoryItemFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Healing", meta=(ClampMin="0.01")) float Healing = 50.f;
	UPROPERTY(EditDefaultsOnly, Category="Healing", meta=(ClampMin="0.1")) float ActivationDuration = 3.f;
};

/** Weapon-specific executor; generic inventory/UI never calls Combat to equip an item. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGEquipWeaponItemAbility : public UNGItemActivateAbility
{
	GENERATED_BODY()
public:
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
};

UCLASS()
class NEXTGENONE_API UNGActivatingItemEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGActivatingItemEffect();
};

UCLASS()
class NEXTGENONE_API UNGHealingEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGHealingEffect();
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGActivateMedkitAbility : public UNGItemActivateAbility
{
	GENERATED_BODY()
public:
	UNGActivateMedkitAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	UFUNCTION() void FinishActivation();
	UFUNCTION() void InventoryChanged();
	void HealthChanged(const FOnAttributeChangeData& Change);
	void BlockingTagChanged(FGameplayTag Tag, int32 Count);
	void CancelActivation();
	TWeakObjectPtr<UAbilitySystemComponent> ActivationASC;
	TWeakObjectPtr<UNGPlayerInventoryManagerComponent> ActivationInventory;
	TWeakObjectPtr<AActor> ActivationAvatar;
	FDelegateHandle HealthHandle;
	FDelegateHandle TagHandle;
	int32 ActivationSlot = INDEX_NONE;
	int32 ActivationCount = 0;
	float HealingAmount = 0.f;
	FActiveGameplayEffectHandle ActivationEffect;
};
