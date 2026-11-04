#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Components/ALSMantleComponent.h"
#include "NGMantleAbility.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGMantleFinished, bool, bCancelled);

/** Original ALS checks, timeline, motion and animation selection; GAS/network lifecycle bridge only. */
UCLASS(Blueprintable, ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGALSMantleExecutor : public UALSMantleComponent
{
	GENERATED_BODY()
public:
	UNGALSMantleExecutor();
	bool CanStart();
	bool StartFromAbility();
	void CancelFromAbility();
	UPROPERTY(BlueprintAssignable, Category="Mantle") FNGMantleFinished OnMantleFinished;
	UPROPERTY(EditDefaultsOnly, Category="Mantle") bool bAutomaticFallingCatch = true;
	UPROPERTY(EditDefaultsOnly, Category="Mantle", meta=(ClampMin="0.05")) float AutoCatchInterval = .15f;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* ThisTick) override;
	virtual void Server_MantleStart_Implementation(float Height, const FALSComponentAndTransform& Ledge, EALSMantleType Type) override final;
private:
	UFUNCTION(NetMulticast, Reliable) void MulticastAbilityStart(float Height, FALSComponentAndTransform Ledge, EALSMantleType Type);
	UFUNCTION(NetMulticast, Reliable) void MulticastAbilityCancel();
	void MonitorExecution();
	FTimerHandle ExecutionTimer;
	double NextAutoAttempt = 0;
	bool bCheckingOnServer = false;
	bool bRunning = false;
};

/** GAS wrapper; geometry, movement, montage and curves belong solely to the ALS component. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGMantleAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGMantleAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	TWeakObjectPtr<UNGALSMantleExecutor> ExecutionComponent;
	UFUNCTION() void Finished(bool bCancelled);
};
