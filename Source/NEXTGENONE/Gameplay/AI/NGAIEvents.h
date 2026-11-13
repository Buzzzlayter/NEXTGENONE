#pragma once

#include "CoreMinimal.h"

/** Shared AI event names: editor declarations and runtime emission use the same contract. */
namespace NGAIEvents
{
	inline const FName Succeeded(TEXT("Succeeded"));
	inline const FName Failed(TEXT("Failed"));
}
