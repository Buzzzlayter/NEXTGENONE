#pragma once

#include "HFSMStateComponent.h"
#include "NGAIObserveTargetComponent.generated.h"

class ANGAIController;
class UNGAIMemoryComponent;
class APawn;
class AActor;

/** Add to a behavior/state to observe its memory target without enabling aiming or firing. */
UCLASS(EditInlineNew, Blueprintable, meta=(DisplayName="AI Observe Target"))
class NEXTGENONE_API UNGAIObserveTargetComponent : public UHFSMStateComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="AI|Observation") bool bObserveLastKnownLocation = true;
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
protected:
	virtual void BeginDestroy() override;
private:
	void UpdateObservation();
	void StopObservation();
	void ReleaseFocus();
	TWeakObjectPtr<ANGAIController> Controller;
	TWeakObjectPtr<UNGAIMemoryComponent> Memory;
	TWeakObjectPtr<APawn> ExpectedPawn;
	TWeakObjectPtr<AActor> FocusActor;
	FVector FocusPoint = FVector::ZeroVector;
	FDelegateHandle MemoryUpdatedHandle;
	bool bOwnsFocus = false;
};
