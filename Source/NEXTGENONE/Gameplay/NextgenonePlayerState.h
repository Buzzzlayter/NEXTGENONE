#pragma once

#include "CoreMinimal.h"
#include "NGAbilitySystem.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Inventory/NGInventoryManagerComponent.h"
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory") TObjectPtr<UNGInventoryManagerComponent> Inventory;
	UPROPERTY(EditDefaultsOnly, Category="GAS") TObjectPtr<UNGAbilitySet> PlayerAbilitySet;
	UPROPERTY(EditDefaultsOnly, Category="GAS") TSoftObjectPtr<UNGAbilitySet> DefaultPlayerAbilitySet;
	// Fallback only: an Input.Jump grant in PlayerAbilitySet replaces this.
	UPROPERTY(EditDefaultsOnly, Category="GAS") TSubclassOf<UGameplayAbility> JumpAbility;
	// Server-only life boundary; repeated possession/binds never heal or grant twice.
	void StartLife(ANextgenoneCharacter* Pawn);
	// Player-wide cooldown survives ability cancellation, equip and Pawn replacement.
	double NextAllowedFireTime = 0.0;
	double NextAllowedInteractionTime = 0.0;
	double NextAllowedDropTime = 0.0;
	uint32 LifeSerial = 0;
	bool bStarterInventoryGranted=false;
private:
	UPROPERTY() FNGGrantedHandles PlayerHandles;
	TWeakObjectPtr<ANextgenoneCharacter> InitializedPawn;
	bool bPlayerAbilitiesGranted = false;
};
