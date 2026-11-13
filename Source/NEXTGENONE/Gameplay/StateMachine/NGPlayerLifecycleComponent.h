#pragma once

#include "HFSMStateComponent.h"
#include "GameplayTagContainer.h"
#include "NGPlayerLifecycleComponent.generated.h"

class APlayerController;
class APawn;
class UNGCharacterStateMachineComponent;

/** Add to the persistent UI root, above WaitForReady, gameplay and death states. */
UCLASS(EditInlineNew, Blueprintable, meta=(DisplayName="Observe Player Lifecycle"))
class NEXTGENONE_API UNGPlayerLifecycleComponent : public UHFSMStateComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Player Lifecycle") FName DeathEvent = TEXT("Dead");
	UPROPERTY(EditAnywhere, Category="Player Lifecycle") FName PawnChangedEvent = TEXT("PawnChanged");
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
protected:
	virtual void BeginDestroy() override;
private:
	void BindPawn(APawn* Pawn);
	void Unbind();
	void StateChanged(FGameplayTag Previous, FGameplayTag Current);
	UFUNCTION() void PawnChanged(APawn* PreviousPawn, APawn* NewPawn);
	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<APawn> ObservedPawn;
	TWeakObjectPtr<UNGCharacterStateMachineComponent> CharacterMachine;
	FDelegateHandle StateHandle;
};
