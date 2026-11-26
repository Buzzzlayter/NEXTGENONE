#pragma once

#include "AbilitySystem/NGAbilitySystem.h"
#include "UObject/Object.h"
#include "NGCharacterStateResources.generated.h"

class UNGControlStateDefinition;
class UNGCharacterStateMachineComponent;
class UPlayerInputSubsystem;
class APawn;

UCLASS()
class NEXTGENONE_API UNGCharacterStateResources : public UObject
{
	GENERATED_BODY()
public:
	void Enter(APawn* StatePawn, UNGControlStateDefinition* StateDefinition);
	void Exit();
	const UNGControlStateDefinition* GetDefinition() const { return Definition.Get(); }
	void RefreshResources();
	bool OwnsAbility(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const;
	bool IsEntryReady() const { return bEntered && bEntryReady; }
private:
	UNGCharacterStateMachineComponent* GetMachine() const;
	void FailEntry(const FString& Reason);
	bool AcquireGASResources();
	bool ActivateEntryAbilities();
	void ReleaseLocalResources();
	void ReleaseResources();
	UPROPERTY(Transient) TObjectPtr<UNGControlStateDefinition> Definition;
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<UAbilitySystemComponent> GrantedASC;
	TWeakObjectPtr<UPlayerInputSubsystem> LocalInput;
	UPROPERTY(Transient) FNGGrantedHandles Handles;
	TArray<FGuid> ContextHandles;
	FGuid PolicyHandle;
	bool bEntered = false;
	bool bEntryReady = false;
};
