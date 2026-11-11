#pragma once

#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "Templates/Function.h"
#include "TimerManager.h"

class UNGAbilitySystemComponent;
class UGameplayAbility;

/** Value owned by one action state. Holds only its own existing GAS input/spec. */
struct FNGAIAbilityInput
{
	bool Start(UObject* InOwner, UNGAbilitySystemComponent* InASC, FGameplayTag Tag, float HoldDuration,
		TFunction<bool()> IsCurrentRun, TFunction<bool()> CanReleaseInput, TFunction<void(bool)> OnEnded);
	void Reset();
private:
	TWeakObjectPtr<UObject> Owner;
	TWeakObjectPtr<UNGAbilitySystemComponent> ASC;
	TWeakObjectPtr<UGameplayAbility> Instance;
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag InputTag;
	FDelegateHandle ActivatedHandle;
	FDelegateHandle EndedHandle;
	FTimerHandle ReleaseTimer;
	TFunction<void(bool)> Completion;
	TFunction<bool()> CanRelease;
	bool bOwnsInput = false;
	void ReleaseInput();
};
