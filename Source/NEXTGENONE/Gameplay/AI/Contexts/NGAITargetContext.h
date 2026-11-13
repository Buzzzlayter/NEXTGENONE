#pragma once

#include "EnvironmentQuery/EnvQueryContext.h"
#include "NGAITargetContext.generated.h"

/** The selected visible target from the querying bot's existing memory. */
UCLASS()
class NEXTGENONE_API UNGAITargetContext : public UEnvQueryContext
{
	GENERATED_BODY()
public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
