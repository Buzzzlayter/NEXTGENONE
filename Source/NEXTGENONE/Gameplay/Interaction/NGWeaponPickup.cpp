#include "Gameplay/Interaction/NGWeaponPickup.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Inventory/NGInventoryItemDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

void ANGWeaponPickup::OnConstruction(const FTransform& Transform)
{
    if (Definition) ItemClass=Definition->InventoryItemClass.LoadSynchronous();
    Super::OnConstruction(Transform);
}
void ANGWeaponPickup::BeginPlay()
{
    if (HasAuthority() && Definition) ItemClass=Definition->InventoryItemClass.LoadSynchronous();
    Super::BeginPlay();
}
void ANGWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ANGWeaponPickup,Definition);
}
void ANGWeaponPickup::OnRep_Definition() { OnConstruction(GetActorTransform()); }
bool ANGWeaponPickup::TryTakeOnServer(ANextgenoneCharacter* Pawn,float MaxDistance)
{
    auto* System=GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
    return System && System->TryInteract(Pawn,this,MaxDistance,[]() { return true; });
}
