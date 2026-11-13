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
	UPROPERTY(EditAnywhere, BlueprintGetter=GetBehaviorClass, Category="AI|Behavior") TSubclassOf<UNGAIBehavior> BehaviorClass;
	UFUNCTION(BlueprintGetter, Category="AI|Behavior") TSubclassOf<UNGAIBehavior> GetBehaviorClass() const;
	UNGAIMemoryComponent* GetMemory() const { return Memory; }

	/** Server-side action entry point. ExpectedPawn prevents an old task from using a new Avatar. */
	UNGAbilitySystemComponent* GetReadyAbilitySystem(const APawn* ExpectedPawn) const;

protected:
	/** Blueprint owns creation/launch of the selected behavior and all FSM details. */
	UFUNCTION(BlueprintImplementableEvent, Category="AI|Behavior")
	void OnStartBehavior();
	/** Blueprint owns cancellation and graph shutdown; also called during EndPlay. */
	UFUNCTION(BlueprintImplementableEvent, Category="AI|Behavior")
	void OnStopBehavior();
	virtual FVector GetFocalPointOnActor(const AActor* Actor) const override;
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(VisibleAnywhere, Category="AI") TObjectPtr<UNGAIMemoryComponent> Memory;
	UPROPERTY(VisibleAnywhere, Instanced, Category="AI|Perception") TObjectPtr<UAISenseConfig_Sight> SightConfig;
	UPROPERTY(VisibleAnywhere, Instanced, Category="AI|Perception") TObjectPtr<UAISenseConfig_Hearing> HearingConfig;
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
