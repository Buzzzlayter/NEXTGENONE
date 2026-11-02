
#pragma once

#include "CoreMinimal.h"
#include "InputData.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerInputSubsystem, Verbose, All);

UENUM(BlueprintType)
enum class EInputMode : uint8
{
	UI,
	GameAndUI,
	Game
};

UENUM(BlueprintType)
enum class EInputType : uint8
{
	KeyboardMouse,
	Gamepad
};

UENUM(BlueprintType)
enum class EGamepadCursorMode : uint8
{
	Disabled,
	Cursor,
	Grid
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInputTypeChanged, EInputType, InputType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInputModeChanged, EInputMode, InputMode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameplayInputBlockedChanged, bool, bBlocked);

/** A UI/FSM owner's local policy. Gameplay blocking composes across all active requests. */
USTRUCT(BlueprintType)
struct UIMODULE_API FPlayerInputPolicy
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBlockGameplay = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSetInputMode = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EInputMode InputMode = EInputMode::Game;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSetCursorMode = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGamepadCursorMode CursorMode = EGamepadCursorMode::Grid;
};
