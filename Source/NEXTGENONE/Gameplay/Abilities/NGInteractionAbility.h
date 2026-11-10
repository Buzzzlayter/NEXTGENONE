#pragma once

#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "NGInteractionAbility.generated.h"

UCLASS()
class NEXTGENONE_API UNGInteractionCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGInteractionCooldownEffect();
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGInteractionAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGInteractionAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
};
