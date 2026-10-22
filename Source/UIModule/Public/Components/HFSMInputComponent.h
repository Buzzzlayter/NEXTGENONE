

#pragma once

#include "CoreMinimal.h"
#include "Components/HFSMPlayerStateComponent.h"
#include "Input/InputData.h"
#include "HFSMInputComponent.generated.h"

class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UPlayerInputSubsystem;

// ********************************************************************************************************************
//                                             UR5HFSMInputComponent
// ********************************************************************************************************************

UCLASS(DisplayName = "InputComponent")
class UHFSMInputComponent : public UHFSMPlayerStateComponent
{
	GENERATED_BODY()

public:
	virtual bool Enter() override;
	virtual void Exit(FName Event) override;
	virtual void AbortEnter() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = MappingContext)
	bool bSetupMappingContext = true;

	UPROPERTY(EditAnywhere, Category = MappingContext, meta = (EditCondition = "bSetupMappingContext"))
	UInputMappingContext* MappingContext;

	UPROPERTY(EditAnywhere, Category = MappingContext, meta = (EditCondition = "bSetupMappingContext"))
	int32 Priority = 1;

	UPROPERTY(EditDefaultsOnly, Category = Input)
	bool bSetupInputMode = false;

	UPROPERTY(EditDefaultsOnly, Category = Input, meta = (EditCondition = "bSetupInputMode"))
	EInputMode InputMode;

	UPROPERTY(EditDefaultsOnly, Category = Input, meta = (EditCondition = "bSetupInputMode"))
	bool bFlushInput = false;

	UPROPERTY(EditDefaultsOnly, Category = Gamepad)
	bool bSetupCursorMode = false;

	UPROPERTY(EditDefaultsOnly, Category = Gamepad, meta = (EditCondition = "bSetupCursorMode"))
	EGamepadCursorMode DefaultCursorMode = EGamepadCursorMode::Grid;

private:
	void RestoreInput();
	UPlayerInputSubsystem* GetPlayerInputSubsystem() const;
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

	TWeakObjectPtr<UPlayerInputSubsystem> AppliedInputSubsystem;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> AppliedEnhancedInputSubsystem;
	EInputMode PreviousInputMode = EInputMode::Game;
	EGamepadCursorMode PreviousCursorMode = EGamepadCursorMode::Disabled;
	int32 PreviousMappingPriority = 0;
	bool bHadMappingContext = false;
	bool bAppliedMappingContext = false;
};
