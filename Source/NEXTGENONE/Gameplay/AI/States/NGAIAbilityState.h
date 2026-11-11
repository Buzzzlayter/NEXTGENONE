#pragma once

#include "Gameplay/AI/States/NGAIActionState.h"
#include "Gameplay/AI/States/NGAIAbilityInput.h"
#include "NGAIAbilityState.generated.h"

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
	FNGAIAbilityInput AbilityInput;
};
