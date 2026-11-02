#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "NGGameplayInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

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

USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGInputMappingContextEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", meta=(ClampMin="0"))
	int32 Priority = 0;
};

/** Gameplay bindings for a Pawn or a temporary control state. Never contains UI actions. */
UCLASS(BlueprintType, Const, meta=(DisplayName="NG Gameplay Input Config"))
class GAMEPLAYFOUNDATION_API UNGGameplayInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(TitleProperty="MappingContext"))
	TArray<FNGInputMappingContextEntry> MappingContexts;

	/** Contexts activated only by states. Their IA/tag routes are still bound once by the Pawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TArray<TObjectPtr<UInputMappingContext>> StateMappingContexts;

	/** Native routes require a handler on the Pawn/input adapter. This list does not create behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(TitleProperty="InputTag"))
	TArray<FNGTaggedInputAction> NativeInputActions;

	/** Tags must match the InputTag of grants in the existing NG Ability Sets. */
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
