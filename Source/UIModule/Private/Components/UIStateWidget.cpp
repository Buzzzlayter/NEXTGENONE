#include "Components/UIStateWidget.h"

#include "HFSMState.h"

void UUIStateWidget::SetMountedState(UHFSMState* InState)
{
	MountedState = InState;
}

void UUIStateWidget::SendStateEvent(FName EventName)
{
	if (auto* State = MountedState.Get())
	{
		State->TriggerTransitionEvent(EventName);
	}
}
