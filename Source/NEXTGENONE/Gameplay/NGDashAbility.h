#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "NGDashAbility.generated.h"

UCLASS(Blueprintable)
class NEXTGENONE_API UNGDashCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGDashCooldownEffect();
};

/** Forward dash on the gravity plane; motion is a CharacterMovement root-motion source. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGDashAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGDashAbility();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dash", meta=(ClampMin="1", ClampMax="5000")) float Speed = 1600.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dash", meta=(ClampMin="0.05", ClampMax="1")) float Duration = .18f;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
protected:
	// Cosmetic customization on the predicting owner / server, not gameplay authority.
	UFUNCTION(BlueprintImplementableEvent, Category="Dash") void OnDashStarted(FVector Direction);
private:
	UFUNCTION() void FinishDash();
};
