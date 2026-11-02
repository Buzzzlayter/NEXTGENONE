#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NGCharacterDefinition.generated.h"

class UNGAbilitySet;
class UNGGameplayInputConfig;
class UAbilitySystemComponent;
struct FNGGrantedHandles;

/** Base gameplay configuration, independent of ALS and the concrete Pawn class.
 * Assigning an asset does not grant abilities: the Pawn lifecycle integration owns grants/handles.
 */
UCLASS(BlueprintType, Const, meta=(DisplayName="NG Character Definition"))
class GAMEPLAYFOUNDATION_API UNGCharacterDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character|Input")
	TObjectPtr<UNGGameplayInputConfig> InputConfig;

	/** Pawn-scoped base grants. Persistent player and equipment grants keep their existing owners. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character|Abilities")
	TArray<TObjectPtr<UNGAbilitySet>> AbilitySets;

	/** Structural grant validation shared by editor authoring and the authoritative lifecycle. */
	bool ValidateAbilityGrants(FString& Error) const;
	/** Server-only; caller owns Out and must release it before replacing the ASC Avatar. */
	bool GiveAbilitySets(UAbilitySystemComponent* ASC, AActor* Avatar, FNGGrantedHandles& Out, bool bNewLife) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
