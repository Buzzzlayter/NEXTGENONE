#pragma once

#include "HFSMState.h"
#include "NGAIPauseState.generated.h"

class UNGAIBehavior;

/** Wait for a cooldown before the next AI action. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGAIPauseState : public UHFSMState
{
	GENERATED_BODY()
public:
	virtual TArray<FName> GetDeclaredTransitionEvents() const override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Pause", meta=(ExposeOnSpawn="true", ClampMin="0")) float Cooldown = 1.f;
protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category="AI|Context") TObjectPtr<UNGAIBehavior> Context;
	virtual void Enter() override;
	virtual void Tick(float DeltaTime) override;
private:
	float RemainingTime = 0.f;
	bool bCompleted = false;
};
