#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "NGGameplayInputConfig.generated.h"

class UInputAction;

/** Device-independent meaning of an action. Keys, triggers and modifiers belong to IA/IMC assets. */
USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGTaggedInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	FGameplayTag InputTag;
};

/** IA-to-tag routes only. Control-state definitions own mapping contexts. Never contains UI actions. */
UCLASS(BlueprintType, Const, meta=(DisplayName="NG Gameplay Input Config"))
class GAMEPLAYFOUNDATION_API UNGGameplayInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Native routes require a handler on the Pawn/input adapter. This list does not create behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(TitleProperty="InputTag"))
	TArray<FNGTaggedInputAction> NativeInputActions;

	/** Tags match InputTag on state/equipment grants. Native handlers may also activate abilities. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(TitleProperty="InputTag"))
	TArray<FNGTaggedInputAction> AbilityInputActions;

	UFUNCTION(BlueprintPure, Category="Input")
	UInputAction* FindNativeInputAction(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintPure, Category="Input")
	UInputAction* FindAbilityInputAction(FGameplayTag InputTag) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
