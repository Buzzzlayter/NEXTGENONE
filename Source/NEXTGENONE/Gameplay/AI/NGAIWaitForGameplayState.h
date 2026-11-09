#pragma once

#include "HFSMState.h"
#include "NGAIWaitForGameplayState.generated.h"

class ANGAIController;

/** Entry barrier: the Controller emits Ready only after the graph has returned to this state. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGAIWaitForGameplayState : public UHFSMState
{
	GENERATED_BODY()
public:
	virtual TArray<FName> GetDeclaredTransitionEvents() const override;
	virtual void Enter() override;
protected:
	/** Assigned by GCFSM before Enter; also used for editor context validation. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="AI|Context")
	TObjectPtr<ANGAIController> Context;
};
