#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Gameplay/Inventory/NGPlayerInventoryManagerComponent.h"
#include "NextgenonePlayerState.generated.h"

class ANextgenoneCharacter;

UCLASS()
class NEXTGENONE_API ANextgenonePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ANextgenonePlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory") TObjectPtr<UNGPlayerInventoryManagerComponent> Inventory;
	// Server-only life boundary; repeated possession/binds never heal or grant twice.
	bool StartLife(ANextgenoneCharacter* Pawn);
	void ResumePendingLife(ANextgenoneCharacter* PreviousPawn);
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(VisibleAnywhere, Category="GAS") TObjectPtr<UNGAbilitySystemComponent> AbilitySystem;
	UPROPERTY(VisibleAnywhere, Category="GAS") TObjectPtr<UNGHealthSet> HealthSet;
	TWeakObjectPtr<ANextgenoneCharacter> InitializedPawn;
	TWeakObjectPtr<ANextgenoneCharacter> PendingLifePawn;
	bool bChangingLife = false;
	void StateResourcesReady(AActor* Avatar);
};
