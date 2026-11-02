#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "NGPunchAbility.generated.h"

class ANextgenoneCharacter;

UCLASS(Blueprintable)
class NEXTGENONE_API UNGPunchCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGPunchCooldownEffect();
};

/** Server-only unarmed strike. GAS owns damage/cooldown; CharacterMovement owns knockback. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGPunchAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGPunchAbility();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Punch|Animation") TObjectPtr<UAnimMontage> PunchMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Punch", meta=(ClampMin="1", ClampMax="1000")) float Damage = 20.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Punch", meta=(ClampMin="50", ClampMax="200")) float Reach = 140.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Punch", meta=(ClampMin="10", ClampMax="50")) float HitRadius = 28.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Punch", meta=(ClampMin="0", ClampMax="0.4")) float Windup = .12f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Punch", meta=(ClampMin="0", ClampMax="2000")) float KnockbackSpeed = 650.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Punch", meta=(ClampMin="0", ClampMax="600")) float KnockbackLift = 140.f;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	TWeakObjectPtr<ANextgenoneCharacter> StrikePawn;
	bool IsStrikePawnValid(const FGameplayAbilityActorInfo* Info) const;
	UFUNCTION() void ResolveStrike();
};
