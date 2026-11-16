#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

void UNGEquipmentInstance::Initialize(UNGInventoryItemInstance* InItem, UNGEquipmentDefinition* InDefinition, int32 InGeneration)
{
	Item = InItem;
	Definition = InDefinition;
	Slot = InDefinition->SlotTag;
	Generation = InGeneration;
}

APawn* UNGEquipmentInstance::GetAvatar() const { return GetTypedOuter<APawn>(); }
void UNGEquipmentInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Definition); DOREPLIFETIME(ThisClass, Slot);
	DOREPLIFETIME(ThisClass, Generation);
	DOREPLIFETIME_CONDITION(ThisClass, Item, COND_OwnerOnly);
}
void UNGEquipmentInstance::OnRep_Source() { OnSourceReplicated.Broadcast(); }
