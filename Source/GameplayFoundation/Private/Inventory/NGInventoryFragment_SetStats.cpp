// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#include "Inventory/NGInventoryFragment_SetStats.h"

#include "Inventory/NGInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NGInventoryFragment_SetStats)

void UNGInventoryFragment_SetStats::OnInstanceCreated(UNGInventoryItemInstance* Instance) const
{
	for (const auto& KVP : InitialItemStats)
	{
		Instance->AddStatTagStack(KVP.Key, KVP.Value);
	}
}

int32 UNGInventoryFragment_SetStats::GetItemStatByTag(FGameplayTag Tag) const
{
	if (const int32* StatPtr = InitialItemStats.Find(Tag))
	{
		return *StatPtr;
	}

	return 0;
}
