#pragma once

#include "AbilitySystem/NGAbilitySystem.h"
#include "NGPlayerAbilitySystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FNGAbilitySpecsReplicated, const UAbilitySystemComponent*);

/** The project's single PlayerState ASC. World interaction routing stays outside GameplayFoundation. */
UCLASS()
class NEXTGENONE_API UNGPlayerAbilitySystemComponent : public UNGAbilitySystemComponent
{
	GENERATED_BODY()

public:
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	/** Emitted before GAS retries pending activations and after processing replicated specs. */
	FNGAbilitySpecsReplicated OnAbilitySpecsReplicated;
	uint32 GetAvatarGeneration() const { return AvatarGeneration; }
	virtual void AbilityInputTagPressed(FGameplayTag InputTag) override;
	virtual bool TryActivateAbilityByInputTag(FGameplayTag InputTag) override;
protected:
	virtual void OnRep_ActivateAbilities() override;
private:
	uint32 AvatarGeneration = 0;
	TWeakObjectPtr<AActor> LastLifeAvatar;
};
