

#pragma once

#include "CoreMinimal.h"
#include "HFSMStateComponent.h"
#include "Input/InputData.h"
#include "HFSMInputComponent.generated.h"

class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UPlayerInputSubsystem;

// ********************************************************************************************************************
//                                             UR5HFSMInputComponent
// ********************************************************************************************************************

UCLASS(DisplayName = "InputComponent")
class UIMODULE_API UHFSMInputComponent : public UHFSMStateComponent
{
	GENERATED_BODY()

public:
	virtual void Enter() override;
	virtual void Exit(FName Event) override;

protected:
	UPROPERTY(EditAnywhere, Category=Input)
	bool bBlockGameplayInput = false;
	UPROPERTY(EditAnywhere, Category = MappingContext)
	bool bSetupMappingContext = true;

	UPROPERTY(EditAnywhere, Category = MappingContext, meta = (EditCondition = "bSetupMappingContext"))
	UInputMappingContext* MappingContext;

	UPROPERTY(EditAnywhere, Category = MappingContext, meta = (EditCondition = "bSetupMappingContext"))
	int32 Priority = 1;

	UPROPERTY(EditAnywhere, Category = Input)
	bool bSetupInputMode = false;

	UPROPERTY(EditAnywhere, Category = Input, meta = (EditCondition = "bSetupInputMode"))
	EInputMode InputMode;
	
	UPROPERTY(EditAnywhere, Category = Gamepad)
	bool bSetupCursorMode = false;

	UPROPERTY(EditAnywhere, Category = Gamepad, meta = (EditCondition = "bSetupCursorMode"))
	EGamepadCursorMode DefaultCursorMode = EGamepadCursorMode::Grid;

private:
	void RestoreInput();
	UPlayerInputSubsystem* GetPlayerInputSubsystem() const;

	TWeakObjectPtr<UPlayerInputSubsystem> AppliedInputSubsystem;
	FGuid MappingHandle;
	FGuid PolicyHandle;
};
