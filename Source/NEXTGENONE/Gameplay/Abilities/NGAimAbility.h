#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Abilities/NGGameplayAbilityFromEquipment.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "NGAimAbility.generated.h"

NEXTGENONE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputAim);
NEXTGENONE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGWeaponAiming);

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
class NEXTGENONE_API UNGAimAbility : public UNGGameplayAbilityFromEquipment
{
	GENERATED_BODY()
public:
	UNGAimAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
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
