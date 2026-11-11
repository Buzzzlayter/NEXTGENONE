#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Abilities/NGGameplayAbilityFromEquipment.h"
#include "GameplayEffect.h"
#include "NGJetpackAbility.generated.h"

class UNGCharacterMovementComponent;

UCLASS()
class NEXTGENONE_API UNGJetpackStateEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGJetpackStateEffect();
};

/** Hold-to-thrust GAS authorization; CharacterMovement performs predicted propulsion. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGJetpackAbility : public UNGGameplayAbilityFromEquipment
{
	GENERATED_BODY()
public:
	UNGJetpackAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	TWeakObjectPtr<UNGCharacterMovementComponent> JetpackMovement;
	TWeakObjectPtr<UAbilitySystemComponent> JetpackASC;
	FActiveGameplayEffectHandle StateEffect;
	FDelegateHandle BlockingTagHandle;
	UFUNCTION() void HandleInputReleased(float TimeHeld);
	UFUNCTION() void FinishBurn();
	void BlockingTagChanged(FGameplayTag Tag, int32 Count);
};
