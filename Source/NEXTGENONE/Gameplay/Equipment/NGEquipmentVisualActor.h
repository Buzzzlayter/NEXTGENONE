#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NGEquipmentVisualActor.generated.h"

class UNGEquipmentInstance;

/** Replicated actor of an installation. GAS resources remain owned by EquipmentComponent. */
UCLASS(Blueprintable)
class NEXTGENONE_API ANGEquipmentVisualActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ANGEquipmentVisualActor();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void Initialize(UNGEquipmentInstance* InSource, UAbilitySystemComponent* InASC);
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UFUNCTION(BlueprintPure, Category="Equipment") virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UFUNCTION(BlueprintPure, Category="Equipment") UNGEquipmentInstance* GetEquipmentSource() const;
protected:
	virtual void AbilitySystemReady();
	virtual void GameplayTagChanged(FGameplayTag Tag, int32 Count);
	UFUNCTION(BlueprintImplementableEvent, Category="Equipment") void OnVisualReady();
	UFUNCTION(BlueprintImplementableEvent, Category="Equipment") void OnGameplayTagChanged(FGameplayTag Tag, int32 Count);
private:
	UPROPERTY(ReplicatedUsing=OnRep_Source) TObjectPtr<UNGEquipmentInstance> Source;
	UFUNCTION() void OnRep_Source();
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle TagChangedHandle;
};
