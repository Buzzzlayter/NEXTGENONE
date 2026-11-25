#pragma once

#include "CoreMinimal.h"

/** Shared AI event names: editor declarations and runtime emission use the same contract. */
namespace NGAIEvents
{
	inline const FName Succeeded(TEXT("Succeeded"));
	inline const FName Failed(TEXT("Failed"));
	inline const FName Search(TEXT("Search"));
	inline const FName HoldAngle(TEXT("HoldAngle"));
	inline const FName GroundFlank(TEXT("GroundFlank"));
	inline const FName Reload(TEXT("Reload"));
	inline const FName Wait(TEXT("Wait"));
	inline const FName EquipWeapon(TEXT("EquipWeapon"));
	inline const FName EquipJetpack(TEXT("EquipJetpack"));
	inline const FName Fly(TEXT("Fly"));
}
