#pragma once

#include "Gameplay/AI/NGAIActionState.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "NGAIAbilityState.generated.h"

struct FAbilityEndedData;
class UGameplayAbility;

/** Owns one existing InstancedPerActor ability's press/release and waits for its real end. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGAIAbilityState : public UNGAIActionState
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Ability", meta=(ExposeOnSpawn="true")) FGameplayTag InputTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Ability", meta=(ExposeOnSpawn="true", ClampMin="0")) float HoldDuration = .5f;
protected:
	virtual void StartAction() override;
	virtual void ReleaseActionResources() override;
private:
	FGameplayAbilitySpecHandle AbilityHandle;
	TWeakObjectPtr<UGameplayAbility> AbilityInstance;
	FDelegateHandle AbilityEndedHandle;
	FDelegateHandle AbilityActivatedHandle;
	FTimerHandle ReleaseTimer;
	bool bOwnsInput = false;
	void ReleaseInput();
};
