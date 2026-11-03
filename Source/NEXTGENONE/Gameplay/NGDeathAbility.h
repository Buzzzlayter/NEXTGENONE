#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "NGDeathAbility.generated.h"

/** Grant in Dead's ControlStateDefinition, with bActivateOnEnter and no InputTag. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGDeathAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGDeathAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
	UFUNCTION(BlueprintCallable, Category="Death") void StartDeathRagdoll();
protected:
	// Dead presentation is immediate on proxies. A delayed death needs a separate FSM phase.
	// Overrides must call StartDeathRagdoll and EndAbility; cancellation finalizes presentation.
	UFUNCTION(BlueprintNativeEvent, Category="Death") void HandleDeath();
private:
	bool IsValidStateAvatar(const FGameplayAbilityActorInfo* Info, FGameplayAbilitySpecHandle Handle) const;
	TWeakObjectPtr<AActor> DeathPawn;
	TWeakObjectPtr<UAbilitySystemComponent> DeathASC;
	FGameplayTag PresentationTag;
};
UCLASS()
class NEXTGENONE_API UNGDeadStateEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGDeadStateEffect();
};
