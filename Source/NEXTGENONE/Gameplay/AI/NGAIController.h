#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NGAIController.generated.h"

/** Project bot controller. Gameplay stays on the existing Character and PlayerState. */
UCLASS()
class NEXTGENONE_API ANGAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANGAIController();

protected:
	virtual FVector GetFocalPointOnActor(const AActor* Actor) const override;
};
