#pragma once

#include "CoreMinimal.h"

class APawn;
class UAbilitySystemComponent;
struct FHitResult;

namespace NGDamage
{
	UAbilitySystemComponent* FindLivingAbilitySystem(APawn* Target);

	/** Server-only GAS damage; a corpse cannot forward damage to its PlayerState's next Avatar. */
	bool ApplyToPawn(APawn* Target, UAbilitySystemComponent* Source, float Damage, const FHitResult* Hit = nullptr);
}
