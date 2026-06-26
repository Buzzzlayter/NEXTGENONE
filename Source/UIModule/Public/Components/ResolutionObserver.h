#pragma once

#include "HFSMStateComponent.h"

#include "CoreMinimal.h"
#include "ResolutionObserver.generated.h"


UCLASS()
class UResolutionObserver : public UHFSMStateComponent
{
	GENERATED_BODY()
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FResolutionChanged, FIntPoint, Size, float, DPIScale);

public:
	virtual void Enter() override;
	virtual void Exit(FName event) override;
	void ResolutionChanged(FViewport* ViewPort, uint32 Val);

	UPROPERTY(BlueprintAssignable)
	FResolutionChanged OnResolutionChanged;
};
