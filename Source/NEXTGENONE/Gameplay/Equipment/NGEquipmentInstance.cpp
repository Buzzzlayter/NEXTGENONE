#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

APawn* UNGEquipmentInstance::GetAvatar() const { return GetTypedOuter<APawn>(); }
UNGEquipmentComponent* UNGEquipmentInstance::GetEquipment() const
{
	const auto* Pawn = GetAvatar(); return Pawn ? Pawn->FindComponentByClass<UNGEquipmentComponent>() : nullptr;
}
bool UNGEquipmentInstance::IsCurrent() const
{
	const auto* Equipment = GetEquipment(); return bInstalled && Equipment && Equipment->IsCurrent(this);
}
void UNGEquipmentInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Definition); DOREPLIFETIME(ThisClass, Slot);
	DOREPLIFETIME(ThisClass, Generation); DOREPLIFETIME(ThisClass, bInstalled);
	DOREPLIFETIME_CONDITION(ThisClass, Item, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, AbilityHandles, COND_OwnerOnly);
}
void UNGEquipmentInstance::OnRep_Source() { if (auto* Equipment = GetEquipment()) Equipment->RefreshReplicatedState(); }
