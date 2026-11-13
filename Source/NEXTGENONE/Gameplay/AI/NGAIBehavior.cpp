#include "Gameplay/AI/NGAIBehavior.h"

#include "Gameplay/AI/NGAIController.h"

ANGAIController* UNGAIBehavior::GetController() const
{
	return Context.Get();
}
