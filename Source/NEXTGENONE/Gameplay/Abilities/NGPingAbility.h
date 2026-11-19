#pragma once

#include "Abilities/GameplayAbility.h"
#include "Gameplay/UI/NGPingMarkerModel.h"
#include "GameplayEffect.h"
#include "NGPingAbility.generated.h"

class APawn;
class APlayerController;

UCLASS()
class NEXTGENONE_API UNGPingCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNGPingCooldownEffect();
};

UCLASS(Blueprintable, PrioritizeCategories = ("Ping"))
class NEXTGENONE_API UNGPingAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNGPingAbility();

	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;

private:
	bool PlacePing(const APawn& Pawn, const APlayerController& Controller, AActor* Source, FName RecipientGroup) const;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Ping")
	TObjectPtr<UNGPingMarkerModel> MarkerTemplate;

	UPROPERTY(EditDefaultsOnly, Category = "Ping", meta = (ClampMin = "1", Units = "cm"))
	float PingRange = 10000.f;
};
