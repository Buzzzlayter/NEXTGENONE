#include "Gameplay/AI/Contexts/NGAITargetContext.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "GameFramework/Pawn.h"

void UNGAITargetContext::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const UObject* Owner = QueryInstance.Owner.Get();
	const auto* AI = Cast<ANGAIController>(Owner);
	if (const auto* Pawn = Cast<APawn>(Owner)) AI = Cast<ANGAIController>(Pawn->GetController());
	const auto* Memory = AI ? AI->GetMemory() : nullptr;
	const auto* Target = Memory ? Memory->GetTarget() : nullptr;
	if (Target && Target->bVisible && Target->Actor.IsValid())
	{
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Target->Actor.Get());
	}
}
