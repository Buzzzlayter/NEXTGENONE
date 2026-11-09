#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NGAIController.generated.h"

class UNGAbilitySystemComponent;
class UNGCharacterStateMachineComponent;
class UNGAIActionState;

/** Project bot controller. Gameplay stays on the existing Character and PlayerState. */
UCLASS()
class NEXTGENONE_API ANGAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANGAIController();
	UPROPERTY(EditDefaultsOnly, Category="AI|FSM") FName BehaviorMachineName;
	UPROPERTY(EditDefaultsOnly, Category="AI|FSM") FName NotReadyEvent = TEXT("NotReady");

	/** Server-side action entry point. ExpectedPawn prevents an old task from using a new Avatar. */
	UNGAbilitySystemComponent* GetReadyAbilitySystem(const APawn* ExpectedPawn) const;
	bool RegisterAction(UNGAIActionState* Action);
	void UnregisterAction(UNGAIActionState* Action);
	void RefreshGameplay();
	uint64 GetPossessionSerial() const { return PossessionSerial; }

protected:
	virtual FVector GetFocalPointOnActor(const AActor* Actor) const override;
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	friend class UNGAIWaitForGameplayState;
	TWeakObjectPtr<UNGCharacterStateMachineComponent> ObservedCharacterFSM;
	TWeakObjectPtr<UNGAIActionState> ActiveAction;
	FDelegateHandle StateChangedHandle;
	FDelegateHandle ResourcesReadyHandle;
	FTimerHandle ReadinessTimer;
	bool bGameplayReady = false;
	bool bStopping = false;
	bool bWaitingForGraphReset = true;
	uint64 PossessionSerial = 0;
	void EnterGameplayWait();
	void BindPawn();
	void ReleasePawn();
	void SendBehaviorEvent(FName Event);
};
