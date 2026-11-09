#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NGAIController.generated.h"

class UNGAbilitySystemComponent;

/** Project bot controller. Gameplay stays on the existing Character and PlayerState. */
UCLASS()
class NEXTGENONE_API ANGAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANGAIController();

	/** Server-side action entry point. ExpectedPawn prevents an old task from using a new Avatar. */
	UNGAbilitySystemComponent* GetReadyAbilitySystem(const APawn* ExpectedPawn) const;

protected:
	virtual FVector GetFocalPointOnActor(const AActor* Actor) const override;
};
