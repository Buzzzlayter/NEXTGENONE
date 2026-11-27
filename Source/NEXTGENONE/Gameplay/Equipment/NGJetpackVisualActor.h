#pragma once

#include "Gameplay/Equipment/NGEquipmentVisualActor.h"
#include "NGJetpackVisualActor.generated.h"

class UStaticMeshComponent;
class UNiagaraComponent;

UCLASS(Blueprintable)
class NEXTGENONE_API ANGJetpackVisualActor : public ANGEquipmentVisualActor
{
	GENERATED_BODY()
public:
	ANGJetpackVisualActor();
protected:
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Jetpack") TObjectPtr<UStaticMeshComponent> LeftThruster;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Jetpack") TObjectPtr<UStaticMeshComponent> RightThruster;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Jetpack") TObjectPtr<UNiagaraComponent> LeftExhaust;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Jetpack") TObjectPtr<UNiagaraComponent> RightExhaust;
	virtual void AbilitySystemReady() override;
	virtual void GameplayTagChanged(FGameplayTag Tag, int32 Count) override;
private:
	struct FExhaustSpawnRates
	{
		TOptional<float> Thrusters;
		TOptional<float> EnergyCore;

		void Capture(const UNiagaraComponent& Exhaust);
		void Apply(UNiagaraComponent& Exhaust, float RateScale) const;
	};

	void UpdateExhaustSpawnRates();
	void UpdateExhaust();
	FExhaustSpawnRates LeftSpawnRates;
	FExhaustSpawnRates RightSpawnRates;
	float PreviousOwnerYaw = 0.f;
	float SmoothedTurn = 0.f;
	bool bExhaustActive = false;
};
