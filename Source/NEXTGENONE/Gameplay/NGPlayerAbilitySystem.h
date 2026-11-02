#pragma once

#include "AbilitySystem/NGAbilitySystem.h"
#include "NGPlayerAbilitySystem.generated.h"

/** The project's single PlayerState ASC. World interaction routing stays outside GameplayFoundation. */
UCLASS()
class NEXTGENONE_API UNGPlayerAbilitySystemComponent : public UNGAbilitySystemComponent
{
	GENERATED_BODY()

public:
	virtual void AbilityInputTagPressed(FGameplayTag InputTag) override;
	virtual bool TryActivateAbilityByInputTag(FGameplayTag InputTag) override;
};
