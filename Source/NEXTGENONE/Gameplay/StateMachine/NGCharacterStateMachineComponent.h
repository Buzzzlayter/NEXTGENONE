#pragma once

#include "GCFSMReplicationComponent.h"
#include "GameplayTagContainer.h"
#include "NGCharacterStateMachineComponent.generated.h"

class UAbilitySystemComponent;
class UNGCharacterStateResources;

/** Actor-context GCFSM transport and lifecycle. Blueprint graph belongs to the Pawn, not this component. */
UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGCharacterStateMachineComponent : public UGCFSMReplicationComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character FSM") FName MachineName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character FSM") FGameplayTag DeathEvent;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Character FSM") bool QueueServerEvent(FGameplayTag Event);
	void NotifyGameplayReady(UAbilitySystemComponent* ASC);
	void RefreshResources();
	void SuspendResources();
	void NotifyDeath();
	void RegisterResources(UNGCharacterStateResources* Resources);
	void UnregisterResources(UNGCharacterStateResources* Resources);
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	bool StartIfReady();
	bool bEnding = false;
	TWeakObjectPtr<UAbilitySystemComponent> ReadyASC;
	TArray<TWeakObjectPtr<UNGCharacterStateResources>> ActiveResources;
};
