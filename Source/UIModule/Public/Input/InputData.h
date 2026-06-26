
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
