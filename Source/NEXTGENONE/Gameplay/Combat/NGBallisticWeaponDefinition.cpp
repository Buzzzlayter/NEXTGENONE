#include "Gameplay/Combat/NGBallisticWeaponDefinition.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Combat/NGBallisticProjectile.h"
#include "Gameplay/Abilities/NGAimAbility.h"
#include "Gameplay/Abilities/NGWeaponAbilities.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryFragment_SetStats.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UNGBallisticWeaponDefinition::UNGBallisticWeaponDefinition()
{
	AttachSocket = TEXT("VB RHS_ik_hand_gun");
	VisualTransform = FTransform(FRotator::ZeroRotator, FVector(12, 0, 0), FVector(.35, .08, .08));
	ProjectileClass = ANGBallisticProjectile::StaticClass();
	AimAbilityClass = UNGAimAbility::StaticClass();
}

bool UNGBallisticWeaponDefinition::ValidateDefinition(FString& Error) const
{
	if (!Super::ValidateDefinition(Error)) return false;
	auto InRange = [](float Value, float Min, float Max) { return FMath::IsFinite(Value) && Value >= Min && Value <= Max; };
	if (!InRange(MuzzleSpeed, 100.f, 200000.f) || !InRange(GravityScale, 0.f, 10.f)
		|| !InRange(LinearDrag, 0.f, 2.f) || !InRange(SpreadDegrees, 0.f, 20.f)
		|| !InRange(BodyImpactImpulse, 0.f, 10000.f) || PelletsPerShot < 1 || PelletsPerShot > 16)
	{ Error = TEXT("Weapon has non-finite or out-of-range ballistics settings."); return false; }
	if (!ProjectileClass || ProjectileClass->HasAnyClassFlags(CLASS_Abstract)
		|| !AimAbilityClass || AimAbilityClass->HasAnyClassFlags(CLASS_Abstract) || AmmoItemClass.IsNull())
	{ Error = TEXT("Weapon requires a concrete projectile, aim ability and ammo item class."); return false; }
	Error.Reset(); return true;
}
void UNGBallisticWeaponDefinition::ResolveResources(TArray<FNGEquipmentAbilityGrant>& OutAbilities, TArray<TSubclassOf<UGameplayEffect>>& OutEffects) const
{
	Super::ResolveResources(OutAbilities, OutEffects);
	auto Add = [&](TSubclassOf<UGameplayAbility> Ability, FGameplayTag Input)
	{ FNGEquipmentAbilityGrant Entry; Entry.Grant.Ability = Ability; Entry.Grant.InputTag = Input; OutAbilities.Add(Entry); };
	if (FireAbilityClass.IsNull()) Add(UNGFireAbility::StaticClass(), TAG_NGFireInput);
	if (ReloadAbilityClass.IsNull()) Add(UNGReloadAbility::StaticClass(), TAG_NGReloadInput);
	if (!AimAbilityClass) Add(UNGAimAbility::StaticClass(), TAG_NGInputAim);
}
#if WITH_EDITOR
EDataValidationResult UNGBallisticWeaponDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const auto Parent = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateDefinition(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	bool bValid = Parent != EDataValidationResult::Invalid;
	UClass* AmmoClass = AmmoItemClass.LoadSynchronous();
	if (!AmmoClass || AmmoClass->HasAnyClassFlags(CLASS_Abstract))
	{ Context.AddError(FText::FromString(TEXT("Ammo item must resolve to a concrete class."))); bValid = false; }
	else
	{
		const auto* Ammo = GetDefault<UNGInventoryItemDefinition>(AmmoClass);
		if (!Ammo->ValidateDefinition(Error)) { Context.AddError(FText::FromString(Error)); bValid = false; }
		for (const UNGInventoryItemFragment* Fragment : Ammo->Fragments)
			if (const auto* Stats = Cast<UNGInventoryFragment_SetStats>(Fragment); Stats && !Stats->InitialItemStats.IsEmpty())
			{ Context.AddError(FText::FromString(TEXT("Reserve ammo must not have instance stats."))); bValid = false; }
	}
	for (const auto& Ability : {FireAbilityClass, ReloadAbilityClass})
		if (!Ability.IsNull())
		{
			UClass* Class = Ability.LoadSynchronous();
			if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
			{ Context.AddError(FText::FromString(TEXT("Configured weapon ability must resolve to a concrete class."))); bValid = false; }
		}
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
