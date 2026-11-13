#pragma once

#include "HFSMState.h"
#include "NGAIBehavior.generated.h"

class ANGAIController;

/** Behavior state hosted by the shared Controller Blueprint FSM. */
UCLASS(Abstract, Blueprintable, BlueprintType)
class NEXTGENONE_API UNGAIBehavior : public UHFSMState
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="AI|Behavior")
	ANGAIController* GetController() const;
protected:
	/** GCFSM assigns this before Enter; the editor uses its type to validate nested task contexts. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="AI|Context")
	TObjectPtr<ANGAIController> Context;
};
