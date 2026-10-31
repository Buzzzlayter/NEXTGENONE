#pragma once

#include "CoreMinimal.h"
#include "NGAbilitySystem.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "NextgenonePlayerState.generated.h"

UCLASS()
class NEXTGENONE_API ANextgenonePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ANextgenonePlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UNGAbilitySystemComponent> AbilitySystem;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UNGHealthSet> HealthSet;
	UPROPERTY(EditDefaultsOnly, Category="GAS") TObjectPtr<UNGAbilitySet> PlayerAbilitySet;
	// Server-only life boundary; repeated possession/binds never heal or grant twice.
	void StartLife(ANextgenoneCharacter* Pawn);
	// Player-wide cooldown survives ability cancellation, equip and Pawn replacement.
	double NextAllowedFireTime = 0.0;
	uint32 LifeSerial = 0;
private:
	UPROPERTY() FNGGrantedHandles PlayerHandles;
	TWeakObjectPtr<ANextgenoneCharacter> InitializedPawn;
	bool bPlayerAbilitiesGranted = false;
};
