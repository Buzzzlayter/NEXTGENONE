#pragma once

#include "Abilities/GameplayAbility.h"
#include "NGCameraShoulderAbility.generated.h"

UCLASS()
class NEXTGENONE_API UNGCameraShoulderAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNGCameraShoulderAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
};
