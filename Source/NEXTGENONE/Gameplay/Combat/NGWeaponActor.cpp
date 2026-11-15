#include "Gameplay/Combat/NGWeaponActor.h"

#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "Gameplay/Combat/NGBallisticProjectile.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/Combat/NGWeaponVFX.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
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

UNGInventoryManagerComponent* ANGWeaponActor::GetInventory() const
{
	const auto* Pawn = GetPawn();
	const auto* Player = Pawn ? Pawn->GetPlayerState() : nullptr;
	return Player ? Player->FindComponentByClass<UNGInventoryManagerComponent>() : nullptr;
}

bool ANGWeaponActor::IsCurrentWeapon() const
{
	const auto* Installation = GetEquipmentSource();
	const auto* Equipment = Installation ? Installation->GetEquipment() : nullptr;
	return Installation && Installation->IsCurrent() && Installation->GetAvatar() == GetPawn()
		&& Equipment && Equipment->GetVisualActor(Installation->GetSlot()) == this;
}

int32 ANGWeaponActor::GetMagazineAmmo() const
{
	const auto* Item = GetItem();
	return Item ? Item->GetStatTagStackCount(TAG_NGMagazineAmmo) : 0;
}

int32 ANGWeaponActor::GetReserveAmmo() const
{
	const auto* Definition = GetDefinition();
	auto* Inventory = GetInventory();
	auto* AmmoClass = Definition ? Definition->AmmoItemClass.LoadSynchronous() : nullptr;
	return Inventory && AmmoClass ? Inventory->GetTotalItemCountByDefinition(AmmoClass) : 0;
}

void ANGWeaponActor::RequestFire()
{
	auto* Pawn = GetPawn();
	if (!Pawn || !Pawn->IsLocallyControlled() || !IsCurrentWeapon()) return;
	const auto* Aim = Pawn->FindComponentByClass<UNGAimComponent>();
	FVector Origin, Direction;
	if (!Aim || !Aim->GetViewRay(Origin, Direction)) return;
	if (Pawn->HasAuthority()) ServerRequestFire_Implementation(Origin, Direction, GetEquipmentSource()->GetEquipment()->GetHandsRevision());
	else ServerRequestFire(Origin, Direction, GetEquipmentSource()->GetEquipment()->GetHandsRevision());
}

void ANGWeaponActor::ServerRequestFire_Implementation(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection, int32 EquipmentRevision)
{
	TryFireWithViewOnServer(ViewOrigin, ViewDirection, EquipmentRevision);
}

bool ANGWeaponActor::TryFireWithViewOnServer(const FVector& ViewOrigin, const FVector& ViewDirection, int32 EquipmentRevision)
{
	auto* Pawn = GetPawn();
	auto* ASC = Cast<UNGAbilitySystemComponent>(GetAbilitySystemComponent());
	const auto* Installation = GetEquipmentSource();
	const auto* Aim = Pawn ? Pawn->FindComponentByClass<UNGAimComponent>() : nullptr;
	if (!HasAuthority() || !IsCurrentWeapon() || !ASC || !ASC->HasMatchingGameplayTag(TAG_NGWeaponAiming)
		|| EquipmentRevision != Installation->GetEquipment()->GetHandsRevision()
		|| !Aim || !Aim->ValidateShotView(ViewOrigin, ViewDirection)) return false;
	TGuardValue<TOptional<FTransform>> ViewScope(RequestedShotView,
		TOptional<FTransform>(FTransform(ViewDirection.Rotation(), ViewOrigin)));
	return ASC->TryActivateAbilityByInputTag(TAG_NGFireInput);
}

FVector ANGWeaponActor::GetShotOrigin() const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return GetActorLocation();
	const auto* Movement = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());
	const FVector Up = Movement ? -Movement->GetGravityDirection() : Pawn->GetActorUpVector();
	return Pawn->GetActorLocation() + Up * 45.f + Pawn->GetActorForwardVector() * 30.f;
}

bool ANGWeaponActor::FireOnServer()
{
	auto* Pawn = Cast<APawn>(GetOwner());
	auto* ASC = GetAbilitySystemComponent();
	auto* Installation = GetEquipmentSource();
	auto* Inventory = GetInventory();
	const auto* Def = Installation ? Cast<UNGWeaponDefinition>(Installation->GetDefinition()) : nullptr;
	auto* ShotItem = Installation ? Installation->GetItem() : nullptr;
	if (!HasAuthority() || !IsCurrentWeapon() || !ASC || !Inventory
		|| !NGPlayerContext::GetCurrentASC(Pawn, true) || ASC->HasMatchingGameplayTag(TAG_NGDead)
		|| !ASC->HasMatchingGameplayTag(TAG_NGWeaponAiming)
		|| ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f
		|| !Def || GetMagazineAmmo() <= 0 || !Inventory->Contains(ShotItem)) return false;
	FString WeaponError;
	if (!Def->ValidateDefinition(WeaponError)) return false;
	FVector Eye, ViewDirection;
	if (RequestedShotView.IsSet())
	{
		Eye = RequestedShotView->GetLocation();
		ViewDirection = RequestedShotView->GetRotation().GetForwardVector();
		RequestedShotView.Reset();
	}
	else
	{
		// Remote players must supply a fresh view with their shot request.
		if (Pawn->IsPlayerControlled() && !Pawn->IsLocallyControlled()) return false;
		const auto* Aim = Pawn->FindComponentByClass<UNGAimComponent>();
		if (!Aim || !Aim->GetViewRay(Eye, ViewDirection) || !Aim->ValidateShotView(Eye, ViewDirection)) return false;
	}
	if (!IsCurrentWeapon() || GetMagazineAmmo() < 1) return false;
	ShotItem->RemoveStatTagStack(TAG_NGMagazineAmmo, 1);
	const bool bFired = IsCurrentWeapon() && ExecuteFire(Eye, ViewDirection);
	if (!bFired) ShotItem->AddStatTagStack(TAG_NGMagazineAmmo, 1);
	return bFired;
}

bool ANGWeaponActor::ExecuteFire_Implementation(FVector Eye, FVector ViewDirection)
{
	auto* Installation = GetEquipmentSource();
	auto* Pawn = GetPawn();
	const auto* Def = GetDefinition();
	if (!HasAuthority() || !IsCurrentWeapon() || !Def) return false;
	const auto* AimComponent = Pawn->FindComponentByClass<UNGAimComponent>();
	if (!AimComponent) return false;
	Eye = AimComponent->GetTargetingOrigin(Eye, ViewDirection);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGWeapon), false, Pawn);
	TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult EyeHit;
	const FVector AimEnd = Eye + ViewDirection * Def->Range;
	// Capsule hits for this slice. A server anchor avoids animation tick dependence.
	GetWorld()->LineTraceSingleByChannel(EyeHit, Eye, AimEnd, ECC_Visibility, Params);
	const auto* Movement = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());
	const FVector Up = Movement ? -Movement->GetGravityDirection() : Pawn->GetActorUpVector();
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
		-Up * FMath::Abs(GetWorld()->GetGravityZ()) * Def->GravityScale, Def->MuzzleSpeed,
		Def->SpreadDegrees, Def->LinearDrag, Def->Damage, Def->Range, Def->BodyImpactImpulse, Def->PelletsPerShot, ShotSerial + 1};
	TArray<ANGBallisticProjectile*> Projectiles;
	auto Discard = [&]() { for (auto* Spawned : Projectiles) if (IsValid(Spawned)) Spawned->Destroy(); };
	for (int32 I = 0; I < Shot.Pellets; ++I)
	{
		const FVector Direction = FMath::VRandCone((Shot.End - Shot.Start).GetSafeNormal(), FMath::DegreesToRadians(Shot.Spread));
		const FTransform Transform(Direction.Rotation(), Shot.Start);
		if (!Installation->IsCurrent()) { Discard(); return false; }
		auto* Projectile = GetWorld()->SpawnActorDeferred<ANGBallisticProjectile>(Shot.ProjectileClass, Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (IsValid(Projectile)) Projectiles.Add(Projectile);
		if (!IsValid(Projectile) || !Installation->IsCurrent()) { Discard(); return false; }
		Projectile->Initialize(Pawn, Direction * Shot.Speed, Shot.Gravity, Shot.Drag, Shot.Damage, Shot.Range, Shot.Id, Shot.Impulse);
		Projectile->SetWeaponEffects(Shot.Weapon);
	}
	if (!Installation->IsCurrent())
	{ Discard(); return false; }
	ShotSerial = Shot.Id;
	for (auto* Projectile : Projectiles) if (IsValid(Projectile)) Projectile->FinishSpawning(Projectile->GetActorTransform());
	MulticastShot(Shot.Start, Shot.End, false, Shot.Id, Shot.Weapon);
	Pawn->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("NG_BALLISTIC_SHOT pawn=%s ammo=%d pellets=%d"), *Pawn->GetName(),GetMagazineAmmo(),Shot.Pellets);
	return true;
}

bool ANGWeaponActor::CompleteReloadOnServer()
{
	auto* Installation = GetEquipmentSource();
	auto* Inventory = GetInventory();
	if (!HasAuthority() || bCompletingReload || !IsCurrentWeapon() || !Inventory) return false;
	auto* Item = Installation->GetItem();
	const auto* Def = GetDefinition();
	const auto* ASC = GetAbilitySystemComponent();
	if (!ASC || !NGPlayerContext::GetCurrentASC(Cast<APawn>(GetOwner()), true)
		|| ASC->HasMatchingGameplayTag(TAG_NGDead) || ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f
		|| !Def || !Inventory->Contains(Item)) return false;
	const int32 Amount = FMath::Min(FMath::Max(0, Def->MagazineCapacity - GetMagazineAmmo()), GetReserveAmmo());
	if (Amount <= 0) return false;
	UClass* Ammo=Def->AmmoItemClass.LoadSynchronous(); if (!Ammo || !Installation->IsCurrent()) return false;
	bCompletingReload=true; ON_SCOPE_EXIT { bCompletingReload=false; };
	return Inventory->ConsumeItemsByDefinition(Ammo, Amount, [Item, Amount]
	{
		Item->AddStatTagStack(TAG_NGMagazineAmmo, Amount);
	});
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
