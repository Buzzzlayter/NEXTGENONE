#pragma once

#include "Gameplay/AI/States/NGAIActionState.h"
#include "NGAIFindTargetState.generated.h"

/** Wait for the memory to select an eligible hostile, without scanning the world. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGAIFindTargetState : public UNGAIActionState
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Target", meta=(ExposeOnSpawn="true")) bool bRequireVisible = false;
protected:
	virtual void StartAction() override;
	virtual void ReleaseActionResources() override;
private:
	FTimerHandle SearchTimer;
	void CheckTarget();
};
