#include "Gameplay/Combat/NGCombat.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "GameFramework/Character.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Combat/NGWeaponActor.h"

UNGCombatComponent::UNGCombatComponent()
{
	SetIsReplicatedByDefault(true);
}
UNGEquipmentComponent* UNGCombatComponent::GetEquipmentComponent() const
{
	return GetOwner()->FindComponentByClass<UNGEquipmentComponent>();
}

ANGWeaponActor* UNGCombatComponent::GetWeaponActor() const
{
	const auto* Equipment = GetEquipmentComponent();
	return Equipment ? Cast<ANGWeaponActor>(Equipment->GetVisualActor(TAG_NGSlotEquipmentWeapon)) : nullptr;
}

UNGWeaponDefinition* UNGCombatComponent::GetWeaponDefinition(const UNGInventoryItemDefinition* ItemDefinition)
{
	return ItemDefinition ? Cast<UNGWeaponDefinition>(ItemDefinition->GetEquipmentDefinition()) : nullptr;
}
void UNGCombatComponent::BindInventory(UNGInventoryManagerComponent* Inventory)
{
	if (Inventory && BoundInventory.Get() == Inventory) return;
	if (BoundInventory.IsValid()) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::InventoryChanged);
	BoundInventory = Inventory;
	if (Inventory) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::InventoryChanged);
	OnRep_CurrentItem();
}
void UNGCombatComponent::InventoryChanged() { OnRep_Ammo(); }
UNGEquipmentInstance* UNGCombatComponent::GetWeaponInstance() const
{
	const auto* Equipment = GetEquipmentComponent();
	return Equipment ? Equipment->FindInstance(TAG_NGSlotEquipmentWeapon) : nullptr;
}
FNGEquipmentState UNGCombatComponent::GetEquipment() const
{
	FNGEquipmentState Result;
	const auto* Equipment = GetEquipmentComponent();
	if (!Equipment) return Result;
	Result.Revision = Equipment->GetHandsRevision();
	const auto* Source = GetWeaponInstance();
	Result.Definition = Source ? Cast<UNGWeaponDefinition>(Source->GetDefinition()) : nullptr;
	Result.bEquipped = Result.Definition && GetWeaponActor();
	return Result;
}
UNGInventoryItemInstance* UNGCombatComponent::GetEquippedItem() const
{
	const auto* Weapon = GetWeaponInstance();
	return Weapon ? Weapon->GetItem() : nullptr;
}
void UNGCombatComponent::EquipmentChanged()
{
	OnRep_CurrentItem(); OnRep_Equipment();
}
int32 UNGCombatComponent::GetMagazineAmmo() const { return GetEquippedItem() ? GetEquippedItem()->GetStatTagStackCount(TAG_NGMagazineAmmo) : 0; }
int32 UNGCombatComponent::GetReserveAmmo() const
{
	const auto* Weapon = GetWeaponActor();
	return Weapon ? Weapon->GetReserveAmmo() : 0;
}
void UNGCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Equipment = GetEquipmentComponent()) Equipment->OnEquipmentChanged.AddUObject(this, &ThisClass::EquipmentChanged);
	OnRep_Equipment();
}
void UNGCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (auto* Equipment = GetEquipmentComponent()) Equipment->OnEquipmentChanged.RemoveAll(this);
	BindAbilitySystem(nullptr);
	BindInventory(nullptr);
	Super::EndPlay(Reason);
}
bool UNGCombatComponent::IsAiming() const
{
	// Remote presentation has no local Controller; use the bound ASC's replicated state.
	const auto* ASC = ObservedASC.Get();
	return GetEquipment().bEquipped && ASC && ASC->GetAvatarActor() == GetOwner()
		&& !ASC->HasMatchingGameplayTag(TAG_NGDead)
		&& ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) > 0.f
		&& ASC->HasMatchingGameplayTag(TAG_NGWeaponAiming);
}
void UNGCombatComponent::BindAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (ObservedASC.Get() == ASC) return;
	if (auto* Previous = ObservedASC.Get())
		Previous->RegisterGameplayTagEvent(TAG_NGWeaponAiming).Remove(AimingTagHandle);
	AimingTagHandle.Reset();
	ObservedASC = ASC;
	if (ASC) AimingTagHandle = ASC->RegisterGameplayTagEvent(TAG_NGWeaponAiming).AddUObject(this, &ThisClass::AimingTagChanged);
	AimingTagChanged(TAG_NGWeaponAiming, ASC ? ASC->GetTagCount(TAG_NGWeaponAiming) : 0);
}
void UNGCombatComponent::AimingTagChanged(FGameplayTag Tag, int32 Count)
{
	OnPresentationChanged.Broadcast();
	OnAimChanged.Broadcast(IsAiming());
}
void UNGCombatComponent::OnRep_CurrentItem()
{
	auto* Item = BoundInventory.IsValid() ? GetEquippedItem() : nullptr;
	if (BoundItem.Get() != Item)
	{
		if (BoundItem.IsValid()) BoundItem->OnStatsChanged.RemoveDynamic(this, &ThisClass::OnRep_Ammo);
		BoundItem = Item;
		if (Item) Item->OnStatsChanged.AddUniqueDynamic(this, &ThisClass::OnRep_Ammo);
	}
	OnRep_Ammo();
}

void UNGCombatComponent::OnRep_Equipment()
{
	OnPresentationChanged.Broadcast();
	OnEquipmentChanged.Broadcast(GetEquipment());
	OnAimChanged.Broadcast(IsAiming());
}
void UNGCombatComponent::OnRep_Ammo() { OnAmmoChanged.Broadcast(GetMagazineAmmo()); }
