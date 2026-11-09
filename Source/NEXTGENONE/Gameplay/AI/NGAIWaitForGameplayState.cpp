#include "Gameplay/AI/NGAIWaitForGameplayState.h"
#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/NGAIEvents.h"

TArray<FName> UNGAIWaitForGameplayState::GetDeclaredTransitionEvents() const
{
	return {NGAIEvents::Ready};
}

void UNGAIWaitForGameplayState::Enter()
{
	Super::Enter();
	if (auto* Controller = Context.Get()) Controller->EnterGameplayWait();
}
