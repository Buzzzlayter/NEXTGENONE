#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "NGSetupCommandlet.generated.h"


UCLASS()
class UNGSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UNGSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
