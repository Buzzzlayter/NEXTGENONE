#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;

/** Local presentation of a server-confirmed interaction. No inventory or widget types. */
struct FNGInteractionContext
{
	FGameplayTag Type;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<UObject> Source;
	bool IsValid() const { return Type.IsValid() && Target.IsValid() && Source.IsValid(); }
};

DECLARE_MULTICAST_DELEGATE(FNGInteractionContextChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FNGInteractionEnded, UObject*);
