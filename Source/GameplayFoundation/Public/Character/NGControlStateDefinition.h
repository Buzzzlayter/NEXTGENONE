#pragma once

#include "Engine/DataAsset.h"
#include "Input/NGGameplayInputConfig.h"
#include "NGControlStateDefinition.generated.h"

class UNGAbilitySet;
class UGameplayEffect;

/** Temporary resources, independent of the FSM implementation and the concrete Pawn. */
UCLASS(BlueprintType, Const)
class GAMEPLAYFOUNDATION_API UNGControlStateDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") TArray<TObjectPtr<UNGAbilitySet>> AbilitySets;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") TArray<TSubclassOf<UGameplayEffect>> Effects;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") TArray<FNGInputMappingContextEntry> MappingContexts;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") bool bBlockGameplayInput = false;
	bool ValidateResources(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
