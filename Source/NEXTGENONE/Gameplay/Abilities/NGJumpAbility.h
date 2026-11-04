#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NGJumpAbility.generated.h"

class ANextgenoneCharacter;

/** Predicted jump; CharacterMovement owns motion and ALS owns its animation. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGJumpAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGJumpAbility();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	TWeakObjectPtr<ANextgenoneCharacter> JumpPawn;
	UFUNCTION() void OnInputReleased(float TimeHeld);
protected:
	UFUNCTION(BlueprintImplementableEvent, Category="Jump") void OnJumpStarted();
	UFUNCTION(BlueprintImplementableEvent, Category="Jump") void OnJumpStopped(bool bCancelled);
};
