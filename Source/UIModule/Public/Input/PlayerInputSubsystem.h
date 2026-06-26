#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "InputData.h"
#include "PlayerInputSubsystem.generated.h"

class FPreprocessorManager;
class UWidget;

// ********************************************************************************************************************
//                                             UPlayerInputSubsystem
// ********************************************************************************************************************

UCLASS(BlueprintType, DisplayName = "PlayerInputSubsystem")
class UIMODULE_API UPlayerInputSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnInputTypeChanged OnInputTypeChanged;

	UPROPERTY(BlueprintAssignable)
	FOnInputModeChanged OnInputModeChanged;

	static UPlayerInputSubsystem* GetPIS(UWorld* World);

	
	UFUNCTION(BlueprintPure, BlueprintCosmetic, meta = (WorldContext = "WCO", CompactNodeTitle = GetInputSubsystem))
	static UPlayerInputSubsystem* GetInputSubsystem(const UObject* WCO);
	
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (WorldContext = WCO))
	static void SetInputMode(UObject* WCO, EInputMode Mode, bool bFlushInput = true);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void SetWidgetToFocus(UWidget* WidgetToFocus);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (CompactNodeTitle = SetGamepadCursorMode))
	void SetGamepadCursorMode(EGamepadCursorMode Mode);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (CompactNodeTitle = ToggleCursorModes))
	void ToggleCursorModes();

	UFUNCTION(BlueprintPure)
	EInputType GetInputType() const;

	UFUNCTION(BlueprintPure)
	EInputMode GetInputMode() const;

	UFUNCTION(BlueprintPure)
	bool IsGamepad() const;

	UFUNCTION(BlueprintPure)
	bool IsKeyboardMouse() const;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	TSharedPtr<FPreprocessorManager> PreprocessorManager;

	EInputMode InputMode = EInputMode::Game;
	EInputType InputType = EInputType::KeyboardMouse;
	EGamepadCursorMode GamepadInputMode = EGamepadCursorMode::Disabled;

	void InputModeChanged(EInputMode Mode);
	void InputTypeChanged(EInputType Type);

	bool ContainsPreProcessorManager() const;
};
