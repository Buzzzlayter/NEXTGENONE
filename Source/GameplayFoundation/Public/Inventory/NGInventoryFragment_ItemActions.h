#pragma once

#include "Inventory/NGInventoryItemDefinition.h"
#include "NGInventoryFragment_ItemActions.generated.h"

class UGameplayAbility;

/** Server-selected activation ability. UI never decides which ability class to execute. */
UCLASS(DisplayName="Item Actions")
class GAMEPLAYFOUNDATION_API UNGInventoryFragment_ItemActions : public UNGInventoryItemFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Actions") TSubclassOf<UGameplayAbility> ActivationAbility;
};
