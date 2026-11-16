#pragma once

#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Gameplay/Character/NGJetpackMovementSettings.h"
#include "NGJetpackDefinition.generated.h"

UCLASS(BlueprintType)
class NEXTGENONE_API UNGJetpackDefinition : public UNGEquipmentDefinition
{
	GENERATED_BODY()
public:
	UNGJetpackDefinition();
	UPROPERTY(EditDefaultsOnly, Category="Jetpack") FNGJetpackMovementSettings Movement;
	UPROPERTY(EditDefaultsOnly, Category="Jetpack", meta=(ClampMin="0.1", ClampMax="30")) float MaxBurnDuration = 4.f;
	virtual bool ValidateDefinition(FString& Error) const override;
};

/** Wearing and active thrust are independent tags/effects. */
UCLASS()
class NEXTGENONE_API UNGJetpackWearingEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UNGJetpackWearingEffect();
};
