#pragma once

#include "AbilitySystem/NGAbilitySystem.h"
#include "NGPlayerAbilitySystem.generated.h"

/** The project's single PlayerState ASC. World interaction routing stays outside GameplayFoundation. */
UCLASS()
class NEXTGENONE_API UNGPlayerAbilitySystemComponent : public UNGAbilitySystemComponent
{
	GENERATED_BODY()

public:
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	virtual bool AreAbilityTagsBlocked(const FGameplayTagContainer& InAbilityTags) const override;
	uint32 GetAvatarGeneration() const { return AvatarGeneration; }
	bool CanFireNow(const AActor* ExpectedAvatar) const;
	bool TryCommitFire(const AActor* ExpectedAvatar, float Interval);
	bool TryCommitInteraction(const AActor* ExpectedAvatar, float Interval);
	virtual void AbilityInputTagPressed(FGameplayTag InputTag) override;
	virtual bool TryActivateAbilityByInputTag(FGameplayTag InputTag) override;
private:
	bool IsCurrentServerAvatar(const AActor* ExpectedAvatar) const;
	double NextFireTime = 0.0;
	double NextInteractionTime = 0.0;
	uint32 AvatarGeneration = 0;
	TWeakObjectPtr<AActor> LastLifeAvatar;
};
