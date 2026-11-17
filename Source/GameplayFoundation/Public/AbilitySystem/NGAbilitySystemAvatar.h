#pragma once

#include "UObject/Interface.h"
#include "NGAbilitySystemAvatar.generated.h"

UINTERFACE(MinimalAPI)
class UNGAbilitySystemAvatar : public UInterface
{
	GENERATED_BODY()
};

class GAMEPLAYFOUNDATION_API INGAbilitySystemAvatar
{
	GENERATED_BODY()
public:
	virtual bool CanInitializeGameplay() const = 0;
	virtual bool CanReleaseGameplay() const = 0;
	virtual void TryInitializeGameplay() = 0;
	virtual void ReleaseGameplayResources() = 0;
};
