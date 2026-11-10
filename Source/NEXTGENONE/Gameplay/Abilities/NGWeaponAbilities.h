#pragma once

#include "Gameplay/Abilities/NGGameplayAbilityFromWeapon.h"
#include "GameplayEffect.h"
#include "NGWeaponAbilities.generated.h"

/** Shared on the PlayerState ASC; removing a weapon does not clear its fire cooldown. */
UCLASS()
class NEXTGENONE_API UNGFireCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGFireCooldownEffect();
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGReloadAbility : public UNGGameplayAbilityFromWeapon
{
	GENERATED_BODY()
public:
	UNGReloadAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
private:
	UFUNCTION() void FinishReload();
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGFireAbility : public UNGGameplayAbilityFromWeapon
{
	GENERATED_BODY()
public:
	UNGFireAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation) const override;
};
