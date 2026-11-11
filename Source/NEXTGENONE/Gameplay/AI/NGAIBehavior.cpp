#include "Gameplay/AI/NGAIBehavior.h"

#include "Gameplay/AI/NGAIController.h"

ANGAIController* UNGAIBehavior::GetController() const
{
	return Controller.Get();
}

UWorld* UNGAIBehavior::GetWorld() const
{
	return Controller.IsValid() ? Controller->GetWorld() : nullptr;
}

void UNGAIBehavior::Initialize(ANGAIController* InController)
{
	if (!Controller.IsValid() && IsValid(InController) && InController->HasAuthority()) Controller = InController;
}
