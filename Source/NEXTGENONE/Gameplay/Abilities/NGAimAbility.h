#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Abilities/NGGameplayAbilityFromWeapon.h"
#include "GameplayEffect.h"
#include "NGAimAbility.generated.h"


/** Replicated GAS state, also applied predictively by the owning client. */
UCLASS()
class NEXTGENONE_API UNGAimingEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGAimingEffect();
};

/** Hold-to-aim. Equipment grants the ability; ALS/UI observe its effect tag. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGAimAbility : public UNGGameplayAbilityFromWeapon
{
	GENERATED_BODY()
public:
	UNGAimAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	FActiveGameplayEffectHandle AimingEffect;
	FDelegateHandle BlockingTagHandle;
	TWeakObjectPtr<UAbilitySystemComponent> AimingASC;
	UFUNCTION() void OnAimInputReleased(float TimeHeld);
	void BlockingTagChanged(FGameplayTag Tag, int32 Count);
};
