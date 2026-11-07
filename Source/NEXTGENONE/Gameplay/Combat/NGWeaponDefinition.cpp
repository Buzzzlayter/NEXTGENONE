#include "Gameplay/Combat/NGCombat.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Combat/NGBallisticProjectile.h"
#include "Gameplay/Abilities/NGAimAbility.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryFragment_SetStats.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UNGWeaponDefinition::UNGWeaponDefinition()
{
    ProjectileClass = ANGBallisticProjectile::StaticClass();
    AimAbilityClass = UNGAimAbility::StaticClass();
}

bool UNGWeaponDefinition::ValidateDefinition(FString& Error) const
{
    auto InRange = [](float Value, float Min, float Max) { return FMath::IsFinite(Value) && Value >= Min && Value <= Max; };
    if (!InRange(MuzzleSpeed, 100.f, 200000.f) || !InRange(GravityScale, 0.f, 10.f)
        || !InRange(LinearDrag, 0.f, 2.f) || !InRange(SpreadDegrees, 0.f, 20.f)
        || !InRange(BodyImpactImpulse, 0.f, 10000.f) || PelletsPerShot < 1 || PelletsPerShot > 16
        || !InRange(Damage, 1.f, 10000.f) || !InRange(Range, 100.f, 100000.f)
        || !FMath::IsFinite(FireInterval) || FireInterval < .05f || !InRange(ReloadDuration, .1f, 10.f)
        || MagazineCapacity < 1 || InitialReserveAmmo < 0 || VisualTransform.ContainsNaN())
    { Error = TEXT("Weapon has non-finite or out-of-range ballistics/ammo/timing/transform settings."); return false; }
    if (!ProjectileClass || ProjectileClass->HasAnyClassFlags(CLASS_Abstract)
        || !AimAbilityClass || AimAbilityClass->HasAnyClassFlags(CLASS_Abstract) || AmmoItemClass.IsNull())
    { Error = TEXT("Weapon requires a concrete projectile, aim ability and ammo item class."); return false; }
    Error.Reset(); return true;
}
#if WITH_EDITOR
EDataValidationResult UNGWeaponDefinition::IsDataValid(FDataValidationContext& Context) const
{
    const auto Parent = Super::IsDataValid(Context);
    FString Error;
    if (!ValidateDefinition(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
    bool bValid = Parent != EDataValidationResult::Invalid;
    auto CheckItem = [&](const TSoftClassPtr<UNGInventoryItemDefinition>& Ref, bool bWeapon)
    {
        UClass* Class = Ref.LoadSynchronous();
        if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
        { Context.AddError(FText::FromString(TEXT("Item reference must resolve to a concrete class."))); bValid = false; return; }
        const auto* Item = GetDefault<UNGInventoryItemDefinition>(Class);
        if (!Item->ValidateDefinition(Error)) { Context.AddError(FText::FromString(Error)); bValid = false; }
        if (bWeapon && Item->GetEquipmentDefinition() != this)
        { Context.AddError(FText::FromString(TEXT("InventoryItemClass must point back to this weapon via its equipment fragment."))); bValid = false; }
        for (const UNGInventoryItemFragment* Fragment : Item->Fragments)
            if (const auto* Stats = Cast<UNGInventoryFragment_SetStats>(Fragment))
            {
                const int32 Magazine = Stats->GetItemStatByTag(TAG_NGMagazineAmmo);
                if ((bWeapon && Magazine > MagazineCapacity) || (!bWeapon && !Stats->InitialItemStats.IsEmpty()))
                { Context.AddError(FText::FromString(TEXT("Weapon magazine exceeds capacity, or reserve ammo has instance stats and cannot be split/merged."))); bValid = false; }
            }
    };
    CheckItem(AmmoItemClass, false);
    if (!InventoryItemClass.IsNull()) CheckItem(InventoryItemClass, true);
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
