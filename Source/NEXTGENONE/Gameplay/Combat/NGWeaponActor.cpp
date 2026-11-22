#include "Gameplay/Combat/NGWeaponActor.h"

#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "Gameplay/Combat/NGBallisticWeaponDefinition.h"
#include "Gameplay/Combat/NGBallisticProjectile.h"
#include "Gameplay/Combat/NGLaserSightComponent.h"
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
	LaserSightClass = UNGLaserSightComponent::StaticClass();
}

void ANGWeaponActor::BeginPlay()
{
	if (GetNetMode() != NM_DedicatedServer && LaserSightClass && !LaserSightClass->HasAnyClassFlags(CLASS_Abstract))
	{
		LaserSight = NewObject<UNGLaserSightComponent>(this, LaserSightClass, TEXT("LaserSight"), RF_Transient);
		LaserSight->SetupAttachment(WeaponMesh, LaserSightSocket);
		LaserSight->SetRelativeTransform(LaserSightOffset);
		LaserSight->RegisterComponent();
	}
	Super::BeginPlay();
}

void ANGWeaponActor::UpdateAimPresentation(const FVector& ViewOrigin, const FVector& ViewDirection)
{
	if (!LaserSight) return;
	const auto* Definition = GetDefinition();
	FVector Start, End;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGAimPreview), false);
	if (!Definition || !ResolveShotPath(ViewOrigin, ViewDirection, *Definition, Start, End, Params))
	{
		LaserSight->SetVisibility(false);
		return;
	}

	FHitResult Hit;
	const FVector LaserOrigin = LaserSight->GetComponentLocation();
	const FVector PreviewEnd = Start + (End - Start).GetSafeNormal() * UNGLaserSightComponent::MaxProjectionDistance;
	const bool bHitSurface = GetWorld()->LineTraceSingleByChannel(Hit, LaserOrigin, PreviewEnd, ECC_Visibility, Params);
	if (bHitSurface) LaserSight->SetTargetPoint(Hit.ImpactPoint);
	LaserSight->SetVisibility(bHitSurface);
}

void ANGWeaponActor::SetLaserSightVisible(bool bVisible)
{
	if (LaserSight) LaserSight->SetVisibility(bVisible);
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
	const auto* Pawn = GetPawn();
	const auto* Installation = GetEquipmentSource();
	return !IsActorBeingDestroyed() && IsValid(Pawn) && Installation && Installation->GetAvatar() == Pawn
		&& GetAbilitySystemComponent();
}

int32 ANGWeaponActor::GetMagazineAmmo() const
{
	const auto* Item = GetItem();
	return Item ? Item->GetStatTagStackCount(TAG_NGMagazineAmmo) : 0;
}

FVector ANGWeaponActor::GetShotOrigin() const
{
	if (const auto* Definition = GetDefinition(); Definition && WeaponMesh->DoesSocketExist(Definition->MuzzleFXSocket))
		return WeaponMesh->GetSocketLocation(Definition->MuzzleFXSocket);

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return GetActorLocation();
	const FVector Up = OwnerActor->GetActorUpVector();
	return OwnerActor->GetActorLocation() + Up * 45.f + OwnerActor->GetActorForwardVector() * 30.f;
}

bool ANGWeaponActor::FireOnServer(FVector ViewOrigin, FVector ViewDirection)
{
	if (!HasAuthority() || !HasWeaponContext()) return false;
	auto* ShotItem = GetItem();
	if (!ShotItem || ShotItem->GetStatTagStackCount(TAG_NGMagazineAmmo) <= 0) return false;
	ShotItem->RemoveStatTagStack(TAG_NGMagazineAmmo, 1);
	const bool bFired = HasWeaponContext() && ExecuteFire(ViewOrigin, ViewDirection);
	if (!bFired) ShotItem->AddStatTagStack(TAG_NGMagazineAmmo, 1);
	return bFired;
}

bool ANGWeaponActor::ResolveShotPath(const FVector& ViewOrigin, const FVector& ViewDirection, const UNGWeaponDefinition& Definition,
	FVector& OutStart, FVector& OutEnd, FCollisionQueryParams& Params) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return false;

	Params.AddIgnoredActor(OwnerActor);
	TArray<AActor*> Attached;
	OwnerActor->GetAttachedActors(Attached);
	Params.AddIgnoredActors(Attached);
	const float ForwardDistance = FVector::DotProduct(OwnerActor->GetActorLocation() - ViewOrigin, ViewDirection);
	const FVector AimOrigin = ViewOrigin + ViewDirection * FMath::Max(0.f, ForwardDistance);
	const FVector AimEnd = AimOrigin + ViewDirection * Definition.Range;
	FHitResult EyeHit;
	GetWorld()->LineTraceSingleByChannel(EyeHit, AimOrigin, AimEnd, ECC_Visibility, Params);
	OutStart = GetShotOrigin();
	OutEnd = EyeHit.bBlockingHit ? EyeHit.ImpactPoint : AimEnd;
	const float ConvergenceDistance = FMath::Min(Definition.Range, Definition.MinAimConvergenceDistance);
	const float DistanceAhead = FVector::DotProduct(OutEnd - OutStart, ViewDirection);
	if (DistanceAhead < ConvergenceDistance)
		OutEnd += ViewDirection * (ConvergenceDistance - DistanceAhead);

	FHitResult Obstruction;
	return !GetWorld()->LineTraceSingleByChannel(Obstruction, OwnerActor->GetActorLocation(), OutStart, ECC_Visibility, Params);
}

bool ANGWeaponActor::ExecuteFire_Implementation(FVector Eye, FVector ViewDirection)
{
	auto* Pawn = GetPawn();
	auto* Def = Cast<UNGBallisticWeaponDefinition>(GetDefinition());
	if (!HasAuthority() || !HasWeaponContext() || !Def) return false;
	FVector Start, End;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGWeapon), false);
	if (!ResolveShotPath(Eye, ViewDirection, *Def, Start, End, Params) || !HasWeaponContext()) return false;
	struct FShotSnapshot
	{
		UNGWeaponDefinition* Weapon;
		TSubclassOf<ANGBallisticProjectile> ProjectileClass;
		FVector Start, End, Gravity;
		float Speed, Spread, Drag, Damage, Range, Impulse;
		int32 Pellets, Id;
	};
	const FShotSnapshot Shot{Def, Def->ProjectileClass, Start, End,
		FVector(0.f, 0.f, GetWorld()->GetGravityZ()) * Def->GravityScale, Def->MuzzleSpeed,
		Def->SpreadDegrees, Def->LinearDrag, Def->Damage, Def->Range, Def->BodyImpactImpulse, Def->PelletsPerShot, ShotSerial + 1};
	TArray<ANGBallisticProjectile*> Projectiles;
	auto Discard = [&]() { for (auto* Spawned : Projectiles) if (IsValid(Spawned)) Spawned->Destroy(); };
	for (int32 I = 0; I < Shot.Pellets; ++I)
	{
		const FVector Direction = FMath::VRandCone((Shot.End - Shot.Start).GetSafeNormal(), FMath::DegreesToRadians(Shot.Spread));
		const FTransform Transform(Direction.Rotation(), Shot.Start);
		auto* Projectile = GetWorld()->SpawnActorDeferred<ANGBallisticProjectile>(Shot.ProjectileClass, Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (IsValid(Projectile)) Projectiles.Add(Projectile);
		if (!IsValid(Projectile) || !HasWeaponContext()) { Discard(); return false; }
		Projectile->Initialize(Pawn, Direction * Shot.Speed, Shot.Gravity, Shot.Drag, Shot.Damage, Shot.Range, Shot.Id, Shot.Impulse);
		Projectile->SetWeaponEffects(Shot.Weapon);
		if (!HasWeaponContext()) { Discard(); return false; }
	}
	ShotSerial = Shot.Id;
	for (auto* Projectile : Projectiles) if (IsValid(Projectile)) Projectile->FinishSpawning(Projectile->GetActorTransform());
	MulticastShot(Shot.Start, Shot.End, false, Shot.Id, Shot.Weapon);
	Pawn->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("NG_BALLISTIC_SHOT pawn=%s ammo=%d pellets=%d"), *Pawn->GetName(),GetMagazineAmmo(),Shot.Pellets);
	return true;
}

void ANGWeaponActor::NotifyProjectileShot(UNGWeaponDefinition* Definition, const FVector& Start, const FVector& End)
{
	MulticastShot(Start, End, false, ++ShotSerial, Definition);
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
	if (PlaysFireSoundAtMuzzle() && Definition->FireSound) UGameplayStatics::PlaySoundAtLocation(this, Definition->FireSound, Start);
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
