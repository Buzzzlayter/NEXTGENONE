// All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "LayoutLayer.generated.h"

UENUM(BlueprintType)
enum class ELayoutLayer : uint8
{
	// Fullscreen BG Images
	Background = 0,

	// Current window content
	Content,

	// System panels, etc.. Over several windows
	System,

	// Popup notifications
	Popup,

	// Always visible elements (network issues icon, FPS, critical messages, etc )
	Overlay
};
