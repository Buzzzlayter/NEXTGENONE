#pragma once

#include "Gameplay/AI/States/NGAIActionState.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "NGAIQueryState.generated.h"

class UEnvQuery;

/** Run any spatial/actor EQS asset and publish its ordered results for later states. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGAIQueryState : public UNGAIActionState
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|EQS", meta=(ExposeOnSpawn="true")) TObjectPtr<UEnvQuery> QueryTemplate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|EQS", meta=(ExposeOnSpawn="true")) TEnumAsByte<EEnvQueryRunMode::Type> RunMode = EEnvQueryRunMode::SingleResult;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|EQS", meta=(ExposeOnSpawn="true")) TArray<FEnvNamedValue> QueryParams;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|EQS", meta=(ExposeOnSpawn="true")) FName ResultKey = TEXT("MovePosition");
protected:
	virtual void StartAction() override;
	virtual void ReleaseActionResources() override;
private:
	int32 QueryID = INDEX_NONE;
};
