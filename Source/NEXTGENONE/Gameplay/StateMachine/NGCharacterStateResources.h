#pragma once

#include "HFSMStateComponent.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "NGCharacterStateResources.generated.h"

class UNGControlStateDefinition;
class UPlayerInputSubsystem;
class APawn;

/** Add to reusable HFSM states. Server GAS, owning-client input; no Controller requirement for proxies. */
UCLASS(EditInlineNew, Blueprintable, meta=(DisplayName="Character State Resources"))
class NEXTGENONE_API UNGCharacterStateResources : public UHFSMStateComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resources") TObjectPtr<UNGControlStateDefinition> Definition;
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
	void RefreshResources();
	void ReleaseResources();
	bool OwnsAbility(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const;
	bool IsEntryReady() const { return bEntered && bEntryReady; }
private:
	bool AcquireGASResources();
	bool ActivateEntryAbilities();
	void ReleaseLocalResources();
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<UAbilitySystemComponent> GrantedASC;
	TWeakObjectPtr<UPlayerInputSubsystem> LocalInput;
	UPROPERTY(Transient) FNGGrantedHandles Handles;
	TArray<FGuid> ContextHandles;
	FGuid PolicyHandle;
	bool bEntered = false;
	bool bGASAcquired = false;
	bool bEntryActivationAttempted = false;
	bool bEntryReady = false;
};
