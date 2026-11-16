#pragma once

#include "UObject/Interface.h"
#include "NGInventoryItemDefinitionValidator.generated.h"

class UNGInventoryItemDefinition;

UINTERFACE(MinimalAPI)
class UNGInventoryItemDefinitionValidator : public UInterface
{
	GENERATED_BODY()
};

class GAMEPLAYFOUNDATION_API INGInventoryItemDefinitionValidator
{
	GENERATED_BODY()
public:
	virtual bool ValidateItemDefinition(const UNGInventoryItemDefinition& Item, FString& Error) const = 0;
};
