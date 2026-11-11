#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NGAIController.generated.h"

class UNGAbilitySystemComponent;
class UNGCharacterStateMachineComponent;
class UNGAIMemoryComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UNGAIBehavior;
struct FGameplayTag;

/** Project bot controller. Gameplay stays on the existing Character and PlayerState. */
UCLASS()
class NEXTGENONE_API ANGAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANGAIController();
	UPROPERTY(EditAnywhere, Category="AI|Behavior") TSubclassOf<UNGAIBehavior> BehaviorClass;
	UNGAIBehavior* GetBehavior() const { return Behavior; }
	UPROPERTY(EditAnywhere, Category="AI|Observation") bool bObserveTarget = true;
	UNGAIMemoryComponent* GetMemory() const { return Memory; }
	void RefreshObservation();

	/** Server-side action entry point. ExpectedPawn prevents an old task from using a new Avatar. */
	UNGAbilitySystemComponent* GetReadyAbilitySystem(const APawn* ExpectedPawn) const;

protected:
	virtual FVector GetFocalPointOnActor(const AActor* Actor) const override;
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(Transient) TObjectPtr<UNGAIBehavior> Behavior;
	UPROPERTY(VisibleAnywhere, Category="AI") TObjectPtr<UNGAIMemoryComponent> Memory;
	UPROPERTY(VisibleAnywhere, Instanced, Category="AI|Perception") TObjectPtr<UAISenseConfig_Sight> SightConfig;
	UPROPERTY(VisibleAnywhere, Instanced, Category="AI|Perception") TObjectPtr<UAISenseConfig_Hearing> HearingConfig;
	bool bOwnsObservationFocus = false;
	TWeakObjectPtr<UNGCharacterStateMachineComponent> ObservedCharacterFSM;
	FDelegateHandle StateChangedHandle;
	FDelegateHandle ResourcesReadyHandle;
	FDelegateHandle ResourceFailureHandle;
	void BindPawn();
	void ReleasePawn();
	void StopBehavior();
	void TryInitializeGameplay(AActor* Avatar);
	void InitializeGameplay();
	void HandleGameplayStateChanged(FGameplayTag Previous, FGameplayTag Current);
	void UnsubscribeGameplay();
};
