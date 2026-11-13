#pragma once

#include "Gameplay/AI/States/NGAIActionState.h"
#include "Gameplay/AI/States/NGAIAbilityInput.h"
#include "NGAIFireBurstState.generated.h"

class UNGCombatComponent;
class UNGEquipmentInstance;

/** Holds the normal Aim ability and pulses the weapon's ServerOnly Fire ability. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGAIFireBurstState : public UNGAIActionState
{
	GENERATED_BODY()
public:
	UNGAIFireBurstState();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Fire", meta=(ExposeOnSpawn="true", ClampMin="0.1")) float BurstDuration = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Fire", meta=(ExposeOnSpawn="true", ClampMin="0")) float AimSettleTime = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Fire", meta=(ExposeOnSpawn="true", ClampMin="0", ClampMax="45")) float AimToleranceDegrees = 5.f;
protected:
	virtual void StartAction() override;
	virtual void ReleaseActionResources() override;
private:
	FNGAIAbilityInput AimInput;
	TWeakObjectPtr<AActor> BurstTarget;
	TWeakObjectPtr<UNGCombatComponent> Combat;
	TWeakObjectPtr<UNGEquipmentInstance> Weapon;
	FTimerHandle FireTimer;
	double EarliestShotTime = 0.;
	bool bOwnsFireFocus = false;
	int32 ShotsFired = 0;
	void UpdateFire();
	void AimEnded(bool bWasCancelled);
};
