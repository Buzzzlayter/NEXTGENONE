#pragma once

#include "Engine/DataAsset.h"
#include "GameplayCueNotify_Static.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "NGControlStateDefinition.generated.h"

class UGameplayEffect;
class UInputMappingContext;
class UNGGameplayInputConfig;

USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGInputMappingContextEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", meta=(ClampMin="0")) int32 Priority = 0;
};

/** Temporary resources, independent of the FSM implementation and the concrete Pawn. */
UCLASS(BlueprintType, Const)
class GAMEPLAYFOUNDATION_API UNGControlStateDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") FGameplayTag StateTag;
	/** Stateless local presentation restored on proxies when this FSM state becomes relevant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") TSubclassOf<UGameplayCueNotify_Static> PresentationCue;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") TArray<FNGAbilityGrant> Abilities;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") TArray<TSubclassOf<UGameplayEffect>> Effects;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") TArray<FNGInputMappingContextEntry> MappingContexts;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State") bool bBlockGameplayInput = false;
	bool ValidateResources(FString& Error) const;
	/** Validate this state's IMC actions against the owning Pawn's routes; parent states may provide other actions. */
	bool ValidateInputMappings(const UNGGameplayInputConfig* InputConfig, FString& Error) const;
	FGameplayTag RegisterPresentationCue() const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
