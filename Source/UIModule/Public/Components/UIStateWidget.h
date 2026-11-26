#pragma once

#include "Blueprint/UserWidget.h"
#include "UIStateWidget.generated.h"

class UHFSMState;

UCLASS(Abstract, Blueprintable)
class UIMODULE_API UUIStateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMountedState(UHFSMState* InState);

protected:
	UFUNCTION(BlueprintCallable, Category="HFSM", meta=(DisplayName="Send State Event"))
	void SendStateEvent(FName EventName);

private:
	TWeakObjectPtr<UHFSMState> MountedState;
};
