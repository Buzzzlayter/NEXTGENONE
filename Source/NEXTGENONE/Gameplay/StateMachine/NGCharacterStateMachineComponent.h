#pragma once

#include "GCFSMReplicationComponent.h"
#include "GameplayTagContainer.h"
#include "NGCharacterStateMachineComponent.generated.h"

class UAbilitySystemComponent;
class UNGCharacterStateResources;
class UNGControlStateDefinition;
struct FGameplayAbilitySpecHandle;

DECLARE_MULTICAST_DELEGATE_TwoParams(FNGCharacterStateChanged, FGameplayTag, FGameplayTag);
DECLARE_MULTICAST_DELEGATE_OneParam(FNGCharacterStateResourcesReady, AActor*);

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
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Character FSM") void RequestStateTransition(FGameplayTag StateTag, FGameplayTag Event);
	UFUNCTION(BlueprintPure, Category="Character FSM") bool HasStateTag(FGameplayTag StateTag) const;
	bool CanReleaseAvatar() const;
	void NotifyResourcesReady(UNGCharacterStateResources* Resources);
	void NotifyResourceFailure(FGameplayTag StateTag, const FString& Reason);
	bool HasResourceFailure() const { return bResourcesFailed; }
	void RestorePresentation() const;
	const UNGControlStateDefinition* GetAbilityStateDefinition(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const;
	FNGCharacterStateChanged OnStateChanged;
	FNGCharacterStateResourcesReady OnResourcesReady;
	FSimpleMulticastDelegate OnResourceFailure;
	void RegisterResources(UNGCharacterStateResources* Resources);
	void UnregisterResources(UNGCharacterStateResources* Resources);
	UAbilitySystemComponent* GetReadyASC() const;
	/** Local snapshot: a real entered state, all its resource adapters ready, current Avatar. */
	bool IsGameplayReady(FGameplayTag RequiredState) const;
	bool OwnsStateAbility(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	bool StartIfReady();
	void NotifyStateChanged();
	FGameplayTag GetCurrentStateTag() const;
	FGameplayTag RequestedStateTag;
	FGameplayTag RequestedStateEvent;
	UPROPERTY(ReplicatedUsing=OnRep_ResourceFailure) FGameplayTag FailedStateTag;
	FGameplayTag LastNotifiedStateTag;
	bool bEnding = false;
	UPROPERTY(ReplicatedUsing=OnRep_ResourceFailure) bool bResourcesFailed = false;
	UFUNCTION() void OnRep_ResourceFailure();
	TWeakObjectPtr<UAbilitySystemComponent> ReadyASC;
	TArray<TWeakObjectPtr<UNGCharacterStateResources>> ActiveResources;
};
