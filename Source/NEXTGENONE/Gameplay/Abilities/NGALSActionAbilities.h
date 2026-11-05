#pragma once

#include "AbilitySystem/NGAbilitySystem.h"
#include "NGALSActionAbilities.generated.h"

NEXTGENONE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputRoll);
NEXTGENONE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputRagdoll);
NEXTGENONE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGRollActive);
NEXTGENONE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGRagdollActive);
NEXTGENONE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGRagdollAbility);

UCLASS()
class NEXTGENONE_API UNGALSActionCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGALSActionCooldownEffect();
};

UCLASS()
class NEXTGENONE_API UNGRagdollStateEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGRagdollStateEffect();
};

/** Server selects the authored ALS roll animation; clients cannot supply an asset or rate. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGRollAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGRollAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
protected:
	UPROPERTY(EditDefaultsOnly, Category="Roll", meta=(ClampMin="0.1", ClampMax="3")) float PlayRate = 1.15f;
private:
	UFUNCTION() void Completed();
	UFUNCTION() void Interrupted();
};

/** Alive ragdoll lifetime, replicated tag and input-driven get-up are owned by this ability. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGRagdollAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGRagdollAbility();
	static bool CanRequestGetUp(const UAbilitySystemComponent* ASC);
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	FActiveGameplayEffectHandle RagdollEffect;
	TWeakObjectPtr<UAbilitySystemComponent> RagdollASC;
	UFUNCTION() void GetUpPressed(float TimeWaited);
};
