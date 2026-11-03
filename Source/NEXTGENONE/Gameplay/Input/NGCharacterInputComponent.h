#pragma once

#include "Input/NGGameplayInputComponent.h"
#include "NGCharacterInputComponent.generated.h"

class UPlayerInputSubsystem;

/** Project input adapter for ALS/combat. No physical keys or runtime-created input assets. */
UCLASS(ClassGroup=Input, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGCharacterInputComponent : public UNGGameplayInputComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Input|Camera", meta=(ClampMin="0")) float StickYawDegreesPerSecond = 180.f;
	UPROPERTY(EditAnywhere, Category="Input|Camera", meta=(ClampMin="0")) float StickPitchDegreesPerSecond = 120.f;
protected:
	virtual void HandleNativeInput_Implementation(FGameplayTag Tag, const FInputActionValue& Value, ETriggerEvent Phase) override;
	virtual void BindInputPolicy() override;
	virtual void UnbindInputPolicy() override;
private:
	TWeakObjectPtr<UPlayerInputSubsystem> LocalInput;
	TSet<FGameplayTag> ActivatedNativeActions;
	UFUNCTION() void GameplayBlocked(bool bBlocked);
};
