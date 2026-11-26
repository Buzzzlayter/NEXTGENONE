#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NGCharacterStateMachineComponent.generated.h"

class UAbilitySystemComponent;
class UNGCharacterStateResources;
class UNGControlStateDefinition;
struct FGameplayAbilitySpecHandle;

DECLARE_MULTICAST_DELEGATE_TwoParams(FNGCharacterStateChanged, FGameplayTag, FGameplayTag);
DECLARE_MULTICAST_DELEGATE_OneParam(FNGCharacterStateResourcesReady, AActor*);

USTRUCT()
struct FNGCharacterControlState
{
	GENERATED_BODY()

	UPROPERTY() FGameplayTag Tag;
	UPROPERTY() bool bResourceFailure = false;
};

UCLASS(ClassGroup=Gameplay, PrioritizeCategories=("CharacterStates"), meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGCharacterStateMachineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNGCharacterStateMachineComponent();

	void NotifyGameplayReady(UAbilitySystemComponent* ASC);
	void RefreshResources();
	void SuspendResources();
	void NotifyDeath();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Character States") void RequestStateTransition(FGameplayTag StateTag);
	UFUNCTION(BlueprintPure, Category="Character States") bool HasStateTag(FGameplayTag StateTag) const;
	bool CanReleaseAvatar() const;
	void NotifyResourcesReady(UNGCharacterStateResources* Resources);
	void NotifyResourceFailure(const FString& Reason);
	bool HasResourceFailure() const { return ControlState.bResourceFailure || bResourceFailureHandled; }
	void RestorePresentation() const;
	const UNGControlStateDefinition* GetAbilityStateDefinition(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const;
	FNGCharacterStateChanged OnStateChanged;
	FNGCharacterStateResourcesReady OnResourcesReady;
	FSimpleMulticastDelegate OnResourceFailure;
	UAbilitySystemComponent* GetReadyASC() const;
	bool IsGameplayReady(FGameplayTag RequiredState) const;
	bool OwnsStateAbility(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UNGControlStateDefinition* FindDefinition(FGameplayTag StateTag) const;
	void SetStateTag(FGameplayTag StateTag);
	void ReleaseActiveResources();
	void NotifyAbilitySpecsReplicated(const UAbilitySystemComponent* ASC);
	void UnbindAbilitySpecNotifications();
	void NotifyStateChanged();
	void HandleResourceFailure();
	UFUNCTION() void OnRep_ControlState();

	UPROPERTY(EditDefaultsOnly, Category="CharacterStates")
	TArray<TObjectPtr<UNGControlStateDefinition>> StateDefinitions;

	UPROPERTY(ReplicatedUsing=OnRep_ControlState)
	FNGCharacterControlState ControlState;

	UPROPERTY(Transient)
	TObjectPtr<UNGCharacterStateResources> ActiveResources;

	TWeakObjectPtr<UAbilitySystemComponent> ReadyASC;
	FDelegateHandle AbilitySpecsReplicatedHandle;
	FGameplayTag LastNotifiedStateTag;
	bool bApplyingState = false;
	bool bResourceFailureHandled = false;
	bool bEnding = false;
};
