#pragma once

#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "NativeGameplayTags.h"
#include "NGItemUseAbility.generated.h"

class UNGInventoryItemInstance;
class UNGPlayerInventoryManagerComponent;
struct FOnAttributeChangeData;

NEXTGENONE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGUsingItem);
NEXTGENONE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGItemUseAbility);

/** One-shot server grant. SourceObject is the selected item, never the equipped weapon. */
UCLASS(Abstract, Blueprintable)
class NEXTGENONE_API UNGItemUseAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGItemUseAbility();
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
	UPROPERTY(EditDefaultsOnly, Category="Healing", meta=(ClampMin="0.1")) float UseDuration = 3.f;
};

UCLASS()
class NEXTGENONE_API UNGUsingItemEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGUsingItemEffect();
};

UCLASS()
class NEXTGENONE_API UNGHealingEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGHealingEffect();
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGUseMedkitAbility : public UNGItemUseAbility
{
	GENERATED_BODY()
public:
	UNGUseMedkitAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	UFUNCTION() void FinishUse();
	UFUNCTION() void InventoryChanged();
	void HealthChanged(const FOnAttributeChangeData& Change);
	void BlockingTagChanged(FGameplayTag Tag, int32 Count);
	void CancelUse();
	TWeakObjectPtr<UAbilitySystemComponent> UseASC;
	TWeakObjectPtr<UNGPlayerInventoryManagerComponent> UseInventory;
	TWeakObjectPtr<AActor> UseAvatar;
	FDelegateHandle HealthHandle;
	FDelegateHandle TagHandle;
	int32 UseSlot = INDEX_NONE;
	int32 UseCount = 0;
	float HealingAmount = 0.f;
	FActiveGameplayEffectHandle UsingEffect;
};
