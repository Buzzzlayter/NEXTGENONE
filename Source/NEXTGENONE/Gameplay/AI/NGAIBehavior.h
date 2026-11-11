#pragma once

#include "UObject/Object.h"
#include "NGAIBehavior.generated.h"

class ANGAIController;

/** One behavior object per controller; Blueprint owns the graph's start/stop policy. */
UCLASS(Abstract, Blueprintable, BlueprintType)
class NEXTGENONE_API UNGAIBehavior : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="AI|Behavior")
	ANGAIController* GetController() const;
	virtual UWorld* GetWorld() const override;
	void Initialize(ANGAIController* InController);
	UFUNCTION(BlueprintImplementableEvent, Category="AI|Behavior") void Start();
	UFUNCTION(BlueprintImplementableEvent, Category="AI|Behavior") void Stop();
private:
	TWeakObjectPtr<ANGAIController> Controller;
};
