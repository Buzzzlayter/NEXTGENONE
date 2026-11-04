// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from local LyraStarterGame (UE 5.4) for NEXTGENONE / UE 5.8.

#include "Inventory/NGInventoryItemInstance.h"

#include "Inventory/NGInventoryItemDefinition.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Inventory/NGInventoryManagerComponent.h"



#include UE_INLINE_GENERATED_CPP_BY_NAME(NGInventoryItemInstance)



UNGInventoryItemInstance::UNGInventoryItemInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
void UNGInventoryItemInstance::PostInitProperties()
{
	Super::PostInitProperties();
	StatTags.OnReplicatedChange = [this]() { OnStatsChanged.Broadcast(); };
}
FNGItemState UNGInventoryItemInstance::CaptureState(int32 Count) const
{
	FNGItemState Result; Result.ItemClass = ItemDef; Result.Count = Count; Result.StatStacks = StatTags.GetAllCounts(); return Result;
}
void UNGInventoryItemInstance::RestoreStatStacks(const TMap<FGameplayTag, int32>& Counts)
{
	const auto Existing = StatTags.GetAllCounts();
	for (const auto& Pair : Existing) StatTags.RemoveStack(Pair.Key, Pair.Value);
	for (const auto& Pair : Counts) if (Pair.Key.IsValid() && Pair.Value > 0) StatTags.AddStack(Pair.Key, Pair.Value);
}

void UNGInventoryItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, StatTags);
	DOREPLIFETIME(ThisClass, ItemDef);
}



void UNGInventoryItemInstance::AddStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	const auto* Owner = GetTypedOuter<AActor>(); if (!Owner || !Owner->HasAuthority()) return;
	StatTags.AddStack(Tag, StackCount);
	OnStatsChanged.Broadcast();
}

void UNGInventoryItemInstance::RemoveStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	const auto* Owner = GetTypedOuter<AActor>(); if (!Owner || !Owner->HasAuthority()) return;
	StatTags.RemoveStack(Tag, StackCount);
	OnStatsChanged.Broadcast();
}

int32 UNGInventoryItemInstance::GetStatTagStackCount(FGameplayTag Tag) const
{
	return StatTags.GetStackCount(Tag);
}

bool UNGInventoryItemInstance::HasStatTag(FGameplayTag Tag) const
{
	return StatTags.ContainsTag(Tag);
}

void UNGInventoryItemInstance::SetItemDef(TSubclassOf<UNGInventoryItemDefinition> InDef)
{
	ItemDef = InDef;
}
void UNGInventoryItemInstance::OnRep_ItemDef()
{
	OnStatsChanged.Broadcast();
	if (auto* Owner = GetTypedOuter<AActor>())
	{
		TArray<UNGInventoryManagerComponent*> Managers;
		Owner->GetComponents(Managers);
		for (auto* Manager : Managers) if (Manager->Contains(this))
		{
			FNGInventoryChangeMessage Change; Change.InventoryOwner = Manager; Change.Instance = this;
			Change.NewCount = Manager->GetCount(this); Change.SlotIndex = Manager->GetSlotIndex(this);
			Manager->OnInventoryChanged.Broadcast(Change);
		}
	}
}

const UNGInventoryItemFragment* UNGInventoryItemInstance::FindFragmentByClass(TSubclassOf<UNGInventoryItemFragment> FragmentClass) const
{
	if ((ItemDef != nullptr) && (FragmentClass != nullptr))
	{
		return GetDefault<UNGInventoryItemDefinition>(ItemDef)->FindFragmentByClass(FragmentClass);
	}

	return nullptr;
}


