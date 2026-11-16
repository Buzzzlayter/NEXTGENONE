#pragma once

#include "AbilitySystem/NGAbilitySystem.h"
#include "Inventory/NGInventoryItemDefinitionValidator.h"
#include "NGEquipmentDefinition.generated.h"

class ANGEquipmentVisualActor;

USTRUCT()
struct NEXTGENONE_API FNGEquipmentAbilityGrant
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FNGAbilityGrant Grant;
	/** Only server-executed startup can be required. Ordinary input activation is never required. */
	UPROPERTY(EditAnywhere) bool bRequiredStartup = false;
};

UCLASS(BlueprintType)
class NEXTGENONE_API UNGEquipmentAbilitySet : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) TArray<FNGEquipmentAbilityGrant> Abilities;
	/** Infinite, nonperiodic, nonstacking effects owned by this installation. */
	UPROPERTY(EditAnywhere) TArray<TSubclassOf<UGameplayEffect>> Effects;
};

/** Project equipment contract. Placement remains in the PlayerState inventory. */
UCLASS(BlueprintType, Blueprintable)
class NEXTGENONE_API UNGEquipmentDefinition : public UDataAsset, public INGInventoryItemDefinitionValidator
{
	GENERATED_BODY()
public:
	/** Exact typed inventory equipment slot, including Weapon and Back. */
	UPROPERTY(EditDefaultsOnly, Category="Equipment") FGameplayTag SlotTag;
	UPROPERTY(EditDefaultsOnly, Category="Equipment|GAS") TArray<TObjectPtr<UNGEquipmentAbilitySet>> AbilitySets;
	UPROPERTY(EditDefaultsOnly, Category="Equipment|GAS") TArray<FNGEquipmentAbilityGrant> Abilities;
	UPROPERTY(EditDefaultsOnly, Category="Equipment|GAS") TArray<TSubclassOf<UGameplayEffect>> Effects;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Visual") TSubclassOf<ANGEquipmentVisualActor> VisualActorClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Visual") FName AttachSocket;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Visual") FTransform VisualTransform = FTransform::Identity;
	virtual bool ValidateDefinition(FString& Error) const;
	virtual bool ValidateItemDefinition(const UNGInventoryItemDefinition& Item, FString& Error) const override;
	virtual void ResolveResources(TArray<FNGEquipmentAbilityGrant>& OutAbilities, TArray<TSubclassOf<UGameplayEffect>>& OutEffects) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
