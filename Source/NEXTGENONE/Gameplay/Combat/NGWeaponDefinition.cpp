#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "Gameplay/Combat/NGWeaponActor.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryFragment_SetStats.h"

UNGWeaponDefinition::UNGWeaponDefinition()
{
	SlotTag = TAG_NGSlotEquipmentWeapon;
}

bool UNGWeaponDefinition::ValidateDefinition(FString& Error) const
{
	if (!Super::ValidateDefinition(Error)) return false;
	if (SlotTag != TAG_NGSlotEquipmentWeapon)
	{ Error = TEXT("Weapon requires the weapon equipment slot."); return false; }
	if (!VisualActorClass || !VisualActorClass->IsChildOf(ANGWeaponActor::StaticClass()))
	{ Error = TEXT("Weapon requires a WeaponActor class."); return false; }
	if (!FMath::IsFinite(Damage) || Damage < 1.f || Damage > 10000.f
		|| !FMath::IsFinite(Range) || Range < 100.f || Range > 100000.f
		|| !FMath::IsFinite(MinAimConvergenceDistance) || MinAimConvergenceDistance < 1.f
		|| !FMath::IsFinite(FireInterval) || FireInterval < .05f
		|| !FMath::IsFinite(ReloadDuration) || ReloadDuration < .1f || ReloadDuration > 10.f
		|| MagazineCapacity < 1)
	{ Error = TEXT("Weapon has non-finite or out-of-range damage/range/ammo/timing settings."); return false; }
	if (!AmmoItemClass.IsNull())
	{
		UClass* AmmoClass = AmmoItemClass.LoadSynchronous();
		if (!AmmoClass || AmmoClass->HasAnyClassFlags(CLASS_Abstract)
			|| !GetDefault<UNGInventoryItemDefinition>(AmmoClass)->ValidateDefinition(Error))
		{ if (Error.IsEmpty()) Error = TEXT("Configured ammo must resolve to a concrete valid item definition."); return false; }
	}
	Error.Reset();
	return true;
}

void UNGWeaponDefinition::GetReloadAmmoOptions(TArray<FNGWeaponAmmoOption>& OutOptions) const
{
	if (!AmmoItemClass.IsNull()) OutOptions.Add({AmmoItemClass, {}});
}

void UNGWeaponDefinition::ResolveResources(TArray<FNGEquipmentAbilityGrant>& OutAbilities, TArray<TSubclassOf<UGameplayEffect>>& OutEffects) const
{
	Super::ResolveResources(OutAbilities, OutEffects);
	auto Add = [&](TSubclassOf<UGameplayAbility> Ability, FGameplayTag Input)
	{ FNGEquipmentAbilityGrant Entry; Entry.Grant.Ability = Ability; Entry.Grant.InputTag = Input; OutAbilities.Add(Entry); };
	if (!FireAbilityClass.IsNull()) Add(FireAbilityClass.LoadSynchronous(), TAG_NGFireInput);
	if (!ReloadAbilityClass.IsNull()) Add(ReloadAbilityClass.LoadSynchronous(), TAG_NGReloadInput);
	if (AimAbilityClass) Add(AimAbilityClass, TAG_NGInputAim);
}

bool UNGWeaponDefinition::ValidateItemDefinition(const UNGInventoryItemDefinition& Item, FString& Error) const
{
	if (!Super::ValidateItemDefinition(Item, Error)) return false;
	for (const UNGInventoryItemFragment* Fragment : Item.Fragments)
		if (const auto* Stats = Cast<UNGInventoryFragment_SetStats>(Fragment);
			Stats && Stats->GetItemStatByTag(TAG_NGMagazineAmmo) > MagazineCapacity)
		{ Error = TEXT("Weapon item magazine exceeds capacity."); return false; }
	Error.Reset();
	return true;
}
