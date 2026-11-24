#pragma once

#include "Engine/DataAsset.h"
#include "NGCharacterVitalsDefinition.generated.h"

UCLASS(BlueprintType, Const)
class GAMEPLAYFOUNDATION_API UNGCharacterVitalsDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vitals", meta=(ClampMin="1"))
	float MaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vitals", meta=(ClampMin="0"))
	float MaxShield = 100.f;
};
