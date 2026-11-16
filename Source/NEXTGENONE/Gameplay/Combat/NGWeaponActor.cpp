#include "Gameplay/Combat/NGWeaponActor.h"

#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "Gameplay/Combat/NGBallisticProjectile.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/Combat/NGWeaponVFX.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ANGWeaponActor::ANGWeaponActor()
{
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetRootComponent());
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

APawn* ANGWeaponActor::GetPawn() const { return Cast<APawn>(GetOwner()); }

UNGWeaponDefinition* ANGWeaponActor::GetDefinition() const
{
	const auto* Installation = GetEquipmentSource();
	return Installation ? Cast<UNGWeaponDefinition>(Installation->GetDefinition()) : nullptr;
}

UNGInventoryItemInstance* ANGWeaponActor::GetItem() const
{
	const auto* Installation = GetEquipmentSource();
	return Installation ? Installation->GetItem() : nullptr;
}

bool ANGWeaponActor::HasWeaponContext() const
{
	const auto* Installation = GetEquipmentSource();
	return !IsActorBeingDestroyed() && Installation && Installation->GetAvatar() == GetPawn()
		&& GetAbilitySystemComponent();
}

int32 ANGWeaponActor::GetMagazineAmmo() const
{
	const auto* Item = GetItem();
	return Item ? Item->GetStatTagStackCount(TAG_NGMagazineAmmo) : 0;
}

FVector ANGWeaponActor::GetShotOrigin() const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return GetActorLocation();
	const FVector Up = Pawn->GetActorUpVector();
	return Pawn->GetActorLocation() + Up * 45.f + Pawn->GetActorForwardVector() * 30.f;
}

bool ANGWeaponActor::FireOnServer(FVector ViewOrigin, FVector ViewDirection)
{
	auto* Installation = GetEquipmentSource();
	const auto* Def = Installation ? Cast<UNGWeaponDefinition>(Installation->GetDefinition()) : nullptr;
	auto* ShotItem = Installation ? Installation->GetItem() : nullptr;
	if (!HasAuthority() || !HasWeaponContext() || !ShotItem || !Def || GetMagazineAmmo() <= 0) return false;
	FString WeaponError;
	if (!Def->ValidateDefinition(WeaponError)) return false;
	if (!HasWeaponContext() || GetMagazineAmmo() < 1) return false;
	ShotItem->RemoveStatTagStack(TAG_NGMagazineAmmo, 1);
	const bool bFired = HasWeaponContext() && ExecuteFire(ViewOrigin, ViewDirection);
	if (!bFired) ShotItem->AddStatTagStack(TAG_NGMagazineAmmo, 1);
	return bFired;
}

bool ANGWeaponActor::ExecuteFire_Implementation(FVector Eye, FVector ViewDirection)
{
	auto* Pawn = GetPawn();
	const auto* Def = GetDefinition();
	if (!HasAuthority() || !HasWeaponContext() || !Def) return false;
	const auto* AimComponent = Pawn->FindComponentByClass<UNGAimComponent>();
	if (!AimComponent) return false;
	Eye = AimComponent->GetTargetingOrigin(Eye, ViewDirection);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGWeapon), false, Pawn);
	TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult EyeHit;
	const FVector AimEnd = Eye + ViewDirection * Def->Range;
	// Capsule hits for this slice. A server anchor avoids animation tick dependence.
	GetWorld()->LineTraceSingleByChannel(EyeHit, Eye, AimEnd, ECC_Visibility, Params);
	const FVector Start = GetShotOrigin();
	const FVector End = EyeHit.bBlockingHit ? EyeHit.ImpactPoint : AimEnd;
	if (FVector::DotProduct(End - Start, ViewDirection) <= UE_SMALL_NUMBER) return false;
	FHitResult Obstruction;
	if (GetWorld()->LineTraceSingleByChannel(Obstruction,Pawn->GetActorLocation(),Start,ECC_Visibility,Params)) return false;
	struct FShotSnapshot
	{
		UNGWeaponDefinition* Weapon;
		TSubclassOf<ANGBallisticProjectile> ProjectileClass;
		FVector Start, End, Gravity;
		float Speed, Spread, Drag, Damage, Range, Impulse;
		int32 Pellets, Id;
	};
	const FShotSnapshot Shot{const_cast<UNGWeaponDefinition*>(Def), Def->ProjectileClass, Start, End,
		FVector(0.f, 0.f, GetWorld()->GetGravityZ()) * Def->GravityScale, Def->MuzzleSpeed,
		Def->SpreadDegrees, Def->LinearDrag, Def->Damage, Def->Range, Def->BodyImpactImpulse, Def->PelletsPerShot, ShotSerial + 1};
	TArray<ANGBallisticProjectile*> Projectiles;
	auto Discard = [&]() { for (auto* Spawned : Projectiles) if (IsValid(Spawned)) Spawned->Destroy(); };
	for (int32 I = 0; I < Shot.Pellets; ++I)
	{
		const FVector Direction = FMath::VRandCone((Shot.End - Shot.Start).GetSafeNormal(), FMath::DegreesToRadians(Shot.Spread));
		const FTransform Transform(Direction.Rotation(), Shot.Start);
		if (!HasWeaponContext()) { Discard(); return false; }
		auto* Projectile = GetWorld()->SpawnActorDeferred<ANGBallisticProjectile>(Shot.ProjectileClass, Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (IsValid(Projectile)) Projectiles.Add(Projectile);
		if (!IsValid(Projectile) || !HasWeaponContext()) { Discard(); return false; }
		Projectile->Initialize(Pawn, Direction * Shot.Speed, Shot.Gravity, Shot.Drag, Shot.Damage, Shot.Range, Shot.Id, Shot.Impulse);
		Projectile->SetWeaponEffects(Shot.Weapon);
	}
	if (!HasWeaponContext())
	{ Discard(); return false; }
	ShotSerial = Shot.Id;
	for (auto* Projectile : Projectiles) if (IsValid(Projectile)) Projectile->FinishSpawning(Projectile->GetActorTransform());
	MulticastShot(Shot.Start, Shot.End, false, Shot.Id, Shot.Weapon);
	Pawn->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("NG_BALLISTIC_SHOT pawn=%s ammo=%d pellets=%d"), *Pawn->GetName(),GetMagazineAmmo(),Shot.Pellets);
	return true;
}

void ANGWeaponActor::MulticastShot_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, int32 ShotId,UNGWeaponDefinition* ShotWeapon)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	PlayFireEffects(ShotWeapon, Start, End);
	OnShotCosmetic.Broadcast(Start, End, bHit, ShotId);
	if (FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke")))
		UE_LOG(LogTemp, Display, TEXT("NG_COSMETIC pawn=%s shot=%d hit=%d"), *GetNameSafe(GetOwner()), ShotId, bHit);
}

void ANGWeaponActor::PlayFireEffects_Implementation(UNGWeaponDefinition* Definition, FVector Start, FVector End)
{
	if (!Definition || GetNetMode() == NM_DedicatedServer) return;
	if (Definition->WeaponFireAnimation && WeaponMesh->GetSkeletalMeshAsset()) WeaponMesh->PlayAnimation(Definition->WeaponFireAnimation, false);
	if (Definition->FireSound) UGameplayStatics::PlaySoundAtLocation(this, Definition->FireSound, Start);
	NGWeaponVFX::Shot(WeaponMesh, Definition, Start, (End - Start).GetSafeNormal());
}

void ANGWeaponActor::MulticastReload_Implementation(UNGWeaponDefinition* Definition)
{
	if (GetNetMode() != NM_DedicatedServer) PlayReloadEffects(Definition);
}

void ANGWeaponActor::PlayReloadEffects_Implementation(UNGWeaponDefinition* Definition)
{
	if (Definition && Definition->WeaponReloadAnimation && WeaponMesh->GetSkeletalMeshAsset())
		WeaponMesh->PlayAnimation(Definition->WeaponReloadAnimation, false);
}
