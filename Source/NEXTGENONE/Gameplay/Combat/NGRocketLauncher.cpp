#include "Gameplay/Combat/NGRocketLauncher.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Abilities/NGAimAbility.h"
#include "Gameplay/Abilities/NGWeaponAbilities.h"
#include "Gameplay/Combat/NGRocketProjectile.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryItemInstance.h"

UNGRocketLauncherDefinition::UNGRocketLauncherDefinition()
{
	VisualActorClass = ANGRocketLauncher::StaticClass();
	AttachSocket = TEXT("VB RHS_ik_hand_gun");
	RocketClass = ANGRocketProjectile::StaticClass();
	AimAbilityClass = UNGAimAbility::StaticClass();
	FireAbilityClass = UNGFireAbility::StaticClass();
	ReloadAbilityClass = UNGReloadAbility::StaticClass();
	MagazineCapacity = 1;
	bAllowAimWhileReloading = true;
	ReloadDuration = 2.2f;
	FireInterval = 0.85f;
	Damage = 110.f;
	Range = 16000.f;
	MinAimConvergenceDistance = 150.f;
}

bool UNGRocketLauncherDefinition::ValidateDefinition(FString& Error) const
{
	if (!Super::ValidateDefinition(Error)) return false;
	if (!VisualActorClass->IsChildOf(ANGRocketLauncher::StaticClass()) || MagazineCapacity != 1 || AmmoItemClass.IsNull()
		|| !RocketClass || RocketClass->HasAnyClassFlags(CLASS_Abstract))
	{
		Error = TEXT("Rocket launcher requires a launcher actor, a single-round magazine, ammo and a concrete rocket class.");
		return false;
	}
	if (!RocketClass.GetDefaultObject()->ValidateSettings(Error)) return false;

	TSet<FSoftObjectPath> AmmoPaths;
	AmmoPaths.Add(AmmoItemClass.ToSoftObjectPath());
	FGameplayTagContainer LoadedTags;
	for (const auto& Variant : SpecialAmmo)
	{
		if (Variant.ItemClass.IsNull() || AmmoPaths.Contains(Variant.ItemClass.ToSoftObjectPath())
			|| !Variant.LoadedTag.MatchesTag(TAG_NGRocketLoadedAmmo) || Variant.LoadedTag == TAG_NGRocketLoadedAmmo
			|| LoadedTags.HasTagExact(Variant.LoadedTag) || !Variant.RocketClass
			|| Variant.RocketClass->HasAnyClassFlags(CLASS_Abstract))
		{
			Error = TEXT("Each special rocket requires a unique ammo class, a unique loaded rocket tag and a concrete rocket class.");
			return false;
		}
		const auto* AmmoDefinition = Variant.ItemClass.LoadSynchronous();
		if (!AmmoDefinition || AmmoDefinition->HasAnyClassFlags(CLASS_Abstract))
		{
			Error = TEXT("Special rocket ammo must resolve to a concrete item definition.");
			return false;
		}
		if (!GetDefault<UNGInventoryItemDefinition>(AmmoDefinition)->ValidateDefinition(Error)
			|| !Variant.RocketClass.GetDefaultObject()->ValidateSettings(Error)) return false;
		AmmoPaths.Add(Variant.ItemClass.ToSoftObjectPath());
		LoadedTags.AddTag(Variant.LoadedTag);
	}
	return true;
}

void UNGRocketLauncherDefinition::GetReloadAmmoOptions(TArray<FNGWeaponAmmoOption>& OutOptions) const
{
	for (const auto& Variant : SpecialAmmo)
		OutOptions.Add({Variant.ItemClass, Variant.LoadedTag});
	Super::GetReloadAmmoOptions(OutOptions);
}

bool ANGRocketLauncher::ExecuteFire_Implementation(FVector ViewOrigin, FVector ViewDirection)
{
	auto* Shooter = Cast<APawn>(GetOwner());
	auto* Definition = Cast<UNGRocketLauncherDefinition>(GetDefinition());
	const auto* Item = GetItem();
	if (!HasAuthority() || !IsValid(Shooter) || !Definition || !Item) return false;

	auto ProjectileClass = Definition->RocketClass;
	for (const auto& Variant : Definition->SpecialAmmo)
	{
		if (!Item->HasStatTag(Variant.LoadedTag)) continue;
		ProjectileClass = Variant.RocketClass;
		break;
	}
	if (!ProjectileClass) return false;

	FVector Start, End;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGRocketLaunch), false);
	if (!ResolveShotPath(ViewOrigin, ViewDirection, *Definition, Start, End, Params)) return false;
	const FVector Direction = (End - Start).GetSafeNormal();
	if (Direction.IsNearlyZero()) return false;

	const FTransform Transform(Direction.Rotation(), Start);
	auto* Rocket = GetWorld()->SpawnActorDeferred<ANGRocketProjectile>(
		ProjectileClass, Transform, nullptr, Shooter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Rocket) return false;
	if (!Rocket->Initialize(Shooter, Direction, Definition->Damage, Definition->Range))
	{
		Rocket->Destroy();
		return false;
	}
	Rocket->FinishSpawning(Transform);
	NotifyProjectileShot(Definition, Start, End);
	return true;
}
