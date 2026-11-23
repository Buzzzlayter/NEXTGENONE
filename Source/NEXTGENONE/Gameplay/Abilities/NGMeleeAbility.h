#pragma once

#include "Abilities/GameplayAbility.h"
#include "NGMeleeAbility.generated.h"

class UAnimMontage;

UCLASS(Blueprintable, PrioritizeCategories = ("Melee"))
class NEXTGENONE_API UNGMeleeAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNGMeleeAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "0.01", Units = "s"))
	float HitTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "1", Units = "cm"))
	float Reach = 175.f;

	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "1"))
	float Damage = 25.f;

	UFUNCTION() void ApplyHit();
	UFUNCTION() void FinishAttack();
	UFUNCTION() void InterruptAttack();
};
