#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "InputData.h"
#include "InputCoreTypes.h"
#include "PlayerInputSubsystem.generated.h"

class FPreprocessorManager;
class UWidget;
class UInputAction;
class UInputMappingContext;
class APlayerController;

USTRUCT()
struct FPlayerMappingRequest
{
	GENERATED_BODY()
	FGuid Handle;
	TWeakObjectPtr<UObject> Owner;
	UPROPERTY() TObjectPtr<UInputMappingContext> Context;
	int32 Priority = 0;
	bool bGameplay = false;
};

USTRUCT()
struct FPlayerPolicyRequest
{
	GENERATED_BODY()
	FGuid Handle;
	TWeakObjectPtr<UObject> Owner;
	FPlayerInputPolicy Policy;
};

// ********************************************************************************************************************
//                                             UPlayerInputSubsystem
// ********************************************************************************************************************

UCLASS(BlueprintType, DisplayName = "PlayerInputSubsystem")
class UIMODULE_API UPlayerInputSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable) FOnGameplayInputBlockedChanged OnGameplayInputBlockedChanged;
	UFUNCTION(BlueprintPure, Category="Input") bool IsGameplayInputBlocked() const { return bGameplayBlocked; }
	UFUNCTION(BlueprintCallable, Category="Input") FGuid AcquireMappingContext(UObject* Owner, UInputMappingContext* Context, int32 Priority, bool bGameplay);
	UFUNCTION(BlueprintCallable, Category="Input") void ReleaseMappingContext(FGuid Handle);
	UFUNCTION(BlueprintCallable, Category="Input") FGuid AcquireInputPolicy(UObject* Owner, FPlayerInputPolicy Policy);
	UFUNCTION(BlueprintCallable, Category="Input") void ReleaseInputPolicy(FGuid Handle);
	/** Used by focused widgets in UIOnly, where gameplay Enhanced Input callbacks do not run. */
	UFUNCTION(BlueprintPure, Category="Input") bool IsKeyMappedToAction(FKey Key, const UInputAction* Action) const;
	UPROPERTY(BlueprintAssignable)
	FOnInputTypeChanged OnInputTypeChanged;

	UPROPERTY(BlueprintAssignable)
	FOnInputModeChanged OnInputModeChanged;
	
	// Requires a player/state context; a world alone does not identify a player.
	UFUNCTION(BlueprintPure, BlueprintCosmetic, meta = (DefaultToSelf = "WCO", CompactNodeTitle = GetInputSubsystem))
	static UPlayerInputSubsystem* GetInputSubsystem(const UObject* WCO);
	
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (DefaultToSelf = "WCO"))
	static void SetInputMode(UObject* WCO, EInputMode Mode, bool bFlushInput = true);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input")
	bool SetPlayerInputMode(EInputMode Mode, bool bFlushInput = true);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void SetWidgetToFocus(UWidget* WidgetToFocus);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (CompactNodeTitle = SetGamepadCursorMode))
	void SetGamepadCursorMode(EGamepadCursorMode Mode);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (CompactNodeTitle = ToggleCursorModes))
	void ToggleCursorModes();

	UFUNCTION(BlueprintPure, Category = "Input")
	EGamepadCursorMode GetGamepadCursorMode() const { return GamepadInputMode; }

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
	UPROPERTY(Transient) TArray<FPlayerMappingRequest> MappingRequests;
	UPROPERTY(Transient) TArray<FPlayerPolicyRequest> PolicyRequests;
	TMap<TWeakObjectPtr<UInputMappingContext>, int32> OriginalContexts;
	TSet<TWeakObjectPtr<UInputMappingContext>> ManagedContexts;
	EInputMode PolicyBaseMode = EInputMode::Game;
	EGamepadCursorMode PolicyBaseCursor = EGamepadCursorMode::Disabled;
	bool bGameplayBlocked = false;
	TWeakObjectPtr<APlayerController> AppliedInputController;
	void ApplyMappingRequests();
	void ApplyPolicyRequests();
	TSharedPtr<FPreprocessorManager> PreprocessorManager;

	EInputMode InputMode = EInputMode::Game;
	EInputType InputType = EInputType::KeyboardMouse;
	EGamepadCursorMode GamepadInputMode = EGamepadCursorMode::Disabled;

	void InputModeChanged(EInputMode Mode);
	void InputTypeChanged(EInputType Type);
	void ApplyGamepadCursorMode();
};
