#pragma once

#include "CoreMinimal.h"
#include "NGAbilitySystem.h"
#include "Components/ALSMantleComponent.h"
#include "NGMantleAbility.generated.h"

class UALSMantleComponent;

/** Project-owned ALS subclass. Retains Blueprint mantle assets, rejects the legacy ledge RPC. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGALSMantleExecutor : public UALSMantleComponent
{
	GENERATED_BODY()
public:
	void StopExecution();
protected:
	virtual void BeginPlay() override;
	virtual void Server_MantleStart_Implementation(float Height, const FALSComponentAndTransform& Ledge, EALSMantleType Type) override final;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGMantleFinished, bool, bCancelled);

/** Authority probes ledges; existing ALS component remains the only animation/motion executor. */
UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGMantleAdapterComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGMantleAdapterComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* ThisTick) override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mantle") FALSMantleTraceSettings TraceSettings;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mantle") bool bAutomaticFallingCatch = true;
	UPROPERTY(BlueprintAssignable, Category="Mantle") FNGMantleFinished OnMantleFinished;
	bool CanStart() const;
	bool StartOnServer(TFunctionRef<bool()> Commit);
	void CancelOnServer();
private:
	UPROPERTY(Transient) TObjectPtr<UALSMantleComponent> ALS;
	bool bRunning = false;
	double FinishDeadline = 0;
	double NextAutoAttempt = 0;
	bool Probe(float& Height, FALSComponentAndTransform& Ledge, EALSMantleType& Type) const;
	UFUNCTION(NetMulticast, Reliable) void MulticastStart(float Height, FALSComponentAndTransform Ledge, EALSMantleType Type);
	UFUNCTION(NetMulticast, Reliable) void MulticastCancel();
};

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
	TWeakObjectPtr<UNGMantleAdapterComponent> ActiveAdapter;
	UFUNCTION() void Finished(bool bCancelled);
};
