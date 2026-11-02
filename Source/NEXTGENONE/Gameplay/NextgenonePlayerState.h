#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "NextgenonePlayerState.generated.h"

class ANextgenoneCharacter;
class UNGCharacterDefinition;

UCLASS()
class NEXTGENONE_API ANextgenonePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ANextgenonePlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UNGAbilitySystemComponent> AbilitySystem;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UNGHealthSet> HealthSet;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory") TObjectPtr<UNGInventoryManagerComponent> Inventory;
	// Server-only life boundary; repeated possession/binds never heal or grant twice.
	bool StartLife(ANextgenoneCharacter* Pawn);
	void ReleaseCharacterAbilities(ANextgenoneCharacter* ExpectedPawn);
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	// Player-wide cooldown survives ability cancellation, equip and Pawn replacement.
	double NextAllowedFireTime = 0.0;
	double NextAllowedInteractionTime = 0.0;
	double NextAllowedDropTime = 0.0;
	uint32 LifeSerial = 0;
	bool bStarterInventoryGranted=false;
private:
	UPROPERTY() FNGGrantedHandles CharacterHandles;
	TWeakObjectPtr<ANextgenoneCharacter> GrantedPawn;
	TWeakObjectPtr<UNGCharacterDefinition> GrantedDefinition;
	TWeakObjectPtr<ANextgenoneCharacter> InitializedPawn;
	bool bChangingLife = false;
};
