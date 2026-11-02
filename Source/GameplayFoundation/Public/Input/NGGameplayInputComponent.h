#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "InputTriggers.h"
#include "NGGameplayInputComponent.generated.h"

class UNGCharacterDefinition;
class UNGAbilitySystemComponent;
class UEnhancedInputComponent;
class UInputComponent;

/** Device-independent Pawn bindings. The project adapter supplies contexts, UI policy and native routes. */
UCLASS(Blueprintable, ClassGroup=Input, meta=(BlueprintSpawnableComponent))
class GAMEPLAYFOUNDATION_API UNGGameplayInputComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UNGCharacterDefinition> CharacterDefinition;
	UFUNCTION(BlueprintCallable, Category="Input") void InitializeInput(UInputComponent* Input);
	UFUNCTION(BlueprintCallable, Category="Input") void ShutdownInput();
	UFUNCTION(BlueprintCallable, Category="Input") void SetGameplayInputBlocked(bool bBlocked);
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UFUNCTION(BlueprintNativeEvent, Category="Input") void HandleNativeInput(FGameplayTag Tag, const FInputActionValue& Value, ETriggerEvent Phase);
protected:
	virtual void HandleNativeInput_Implementation(FGameplayTag Tag, const FInputActionValue& Value, ETriggerEvent Phase) {}
	virtual void AcquireInputContexts() {}
	virtual void ReleaseInputContexts() {}
	UNGAbilitySystemComponent* GetInputASC() const;
	void ReleaseHeldInput();
private:
	void NativeInput(const FInputActionValue& Value, FGameplayTag Tag, ETriggerEvent Phase);
	void AbilityPressed(const FInputActionValue& Value, FGameplayTag Tag);
	void AbilityReleased(const FInputActionValue& Value, FGameplayTag Tag);
	bool CanProcessInput() const;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	TWeakObjectPtr<UNGAbilitySystemComponent> PressedASC;
	TArray<uint32> BindingHandles;
	TSet<FGameplayTag> HeldAbilities;
	TMap<FGameplayTag, FInputActionValue> HeldNative;
	TSet<FGameplayTag> SuppressedUntilRelease;
	bool bGameplayBlocked = false;
};
