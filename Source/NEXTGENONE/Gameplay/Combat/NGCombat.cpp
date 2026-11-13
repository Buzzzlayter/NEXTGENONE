#include "Gameplay/Combat/NGCombat.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Animation/AnimSequence.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Gameplay/Combat/NGBallisticProjectile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "Gameplay/Combat/NGWeaponVFX.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"


UNGCombatComponent::UNGCombatComponent()
{
	SetIsReplicatedByDefault(true);
}
UNGEquipmentComponent* UNGCombatComponent::GetEquipmentComponent() const
{
	return GetOwner()->FindComponentByClass<UNGEquipmentComponent>();
}
void UNGCombatComponent::SetWeaponMesh(USkeletalMeshComponent* Mesh)
{
	WeaponMesh = Mesh;
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
    Result.bEquipped = Result.Definition != nullptr;
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
	const auto* Def=GetEquipment().Definition.Get(); UClass* Ammo=Def ? Def->AmmoItemClass.LoadSynchronous() : nullptr;
	return BoundInventory.IsValid() && Ammo ? BoundInventory->GetTotalItemCountByDefinition(Ammo) : 0;
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

void UNGCombatComponent::RequestFire()
{
	auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !IsAiming()) return;
	const auto* Aim = Pawn->FindComponentByClass<UNGAimComponent>();
	FVector Origin, Direction;
	if (!Aim || !Aim->GetViewRay(Origin, Direction)) return;
	if (Pawn->HasAuthority()) ServerRequestFire_Implementation(Origin, Direction, GetEquipment().Revision);
	else ServerRequestFire(Origin, Direction, GetEquipment().Revision);
}
void UNGCombatComponent::ServerRequestFire_Implementation(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection, int32 EquipmentRevision)
{
	auto* Pawn = Cast<APawn>(GetOwner());
	auto* ASC = Cast<UNGAbilitySystemComponent>(GetCombatASC());
	const auto* Aim = Pawn ? Pawn->FindComponentByClass<UNGAimComponent>() : nullptr;
	if (!ASC || !NGPlayerContext::GetCurrentASC(Pawn, true)
		|| ASC->HasMatchingGameplayTag(TAG_NGDead) || ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f
		|| !GetEquipment().bEquipped
		|| !ASC->HasMatchingGameplayTag(TAG_NGWeaponAiming)
		|| EquipmentRevision != GetEquipment().Revision || !Aim || !Aim->ValidateShotView(ViewOrigin, ViewDirection)) return;
	TGuardValue<TOptional<FTransform>> ViewScope(RequestedShotView,
		TOptional<FTransform>(FTransform(FVector(ViewDirection).Rotation(), ViewOrigin)));
	ASC->TryActivateAbilityByInputTag(TAG_NGFireInput);
}
FVector UNGCombatComponent::GetShotOrigin() const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return GetOwner()->GetActorLocation();
	const auto* Movement = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());
	const FVector Up = Movement ? -Movement->GetGravityDirection() : Pawn->GetActorUpVector();
	return Pawn->GetActorLocation() + Up * 45.f + Pawn->GetActorForwardVector() * 30.f;
}

bool UNGCombatComponent::FireOnServer(UNGEquipmentInstance* Source)
{
	auto* Pawn = Cast<APawn>(GetOwner());
	auto* ASC = GetCombatASC();
	const auto* Def = Source ? Cast<UNGWeaponDefinition>(Source->GetDefinition()) : nullptr;
    auto* ShotItem = Source ? Source->GetItem() : nullptr;
	if (!Source || !Source->IsCurrent() || Source != GetWeaponInstance() || !ASC || !BoundInventory.IsValid()
		|| !NGPlayerContext::GetCurrentASC(Pawn, true) || ASC->HasMatchingGameplayTag(TAG_NGDead)
		|| ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f
		|| !Def || GetMagazineAmmo() <= 0 || !BoundInventory->Contains(ShotItem)) return false;
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
        if (!Source->IsCurrent()) { Discard(); return false; }
        auto* Projectile = GetWorld()->SpawnActorDeferred<ANGBallisticProjectile>(Shot.ProjectileClass, Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (IsValid(Projectile)) Projectiles.Add(Projectile);
        if (!IsValid(Projectile) || !Source->IsCurrent()) { Discard(); return false; }
        Projectile->Initialize(Pawn, this, Direction * Shot.Speed, Shot.Gravity, Shot.Drag, Shot.Damage, Shot.Range, Shot.Id, Shot.Impulse);
        Projectile->SetWeaponEffects(Shot.Weapon);
    }
    if (!Source->IsCurrent() || Source != GetWeaponInstance() || ShotItem->GetStatTagStackCount(TAG_NGMagazineAmmo) < 1)
    { Discard(); return false; }
    ShotSerial = Shot.Id;
    ShotItem->RemoveStatTagStack(TAG_NGMagazineAmmo, 1);
    for (auto* Projectile : Projectiles) if (IsValid(Projectile)) Projectile->FinishSpawning(Projectile->GetActorTransform());
    MulticastShot(Shot.Start, Shot.End, false, Shot.Id, Shot.Weapon);
	OnRep_Ammo(); Pawn->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("NG_BALLISTIC_SHOT pawn=%s ammo=%d pellets=%d"), *Pawn->GetName(),ShotItem->GetStatTagStackCount(TAG_NGMagazineAmmo),Shot.Pellets);
	return true;
}
bool UNGCombatComponent::ApplyDamageToPawn(APawn* Target, UAbilitySystemComponent* Source, float Damage, const FHitResult* Hit)
{
	if (!IsValid(Target) || !Target->HasAuthority() || !Source || !Source->IsOwnerActorAuthoritative() || !FMath::IsFinite(Damage) || Damage <= 0) return false;
	auto* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC || TargetASC->GetAvatarActor() != Target || TargetASC->HasMatchingGameplayTag(TAG_NGDead)
		|| TargetASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f) return false;
	auto Context = Source->MakeEffectContext(); Context.AddSourceObject(Target);
	if (Hit) Context.AddHitResult(*Hit);
	auto Spec = Source->MakeOutgoingSpec(UNGDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid()) return false;
	Spec.Data->SetSetByCallerMagnitude(TAG_NGDamage, Damage);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
	return true;
}
void UNGCombatComponent::OnRep_Equipment()
{
	OnPresentationChanged.Broadcast();
	OnEquipmentChanged.Broadcast(GetEquipment());
	OnAimChanged.Broadcast(IsAiming());
}
void UNGCombatComponent::OnRep_Ammo() { OnAmmoChanged.Broadcast(GetMagazineAmmo()); }
void UNGCombatComponent::MulticastShot_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, int32 ShotId,UNGWeaponDefinition* ShotWeapon)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (const auto* Def = ShotWeapon) if (auto* Pawn = Cast<ACharacter>(GetOwner()))
	{
		if (Def->WeaponFireAnimation && WeaponMesh.IsValid() && WeaponMesh->GetSkeletalMeshAsset()) WeaponMesh->PlayAnimation(Def->WeaponFireAnimation, false);
		if (Def->CharacterFireMontage) Pawn->PlayAnimMontage(Def->CharacterFireMontage);
		if (Def->FireSound) UGameplayStatics::PlaySoundAtLocation(this,Def->FireSound,Start);
		NGWeaponVFX::Shot(WeaponMesh.Get(),Def,Start,(FVector(End)-FVector(Start)).GetSafeNormal());
	}
	OnShotCosmetic.Broadcast(Start, End, bHit, ShotId);
	if (FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke")))
		UE_LOG(LogTemp, Display, TEXT("NG_COSMETIC pawn=%s shot=%d hit=%d"), *GetOwner()->GetName(), ShotId, bHit);
}
void UNGCombatComponent::MulticastProjectileImpact_Implementation(FVector_NetQuantize Point,FVector_NetQuantizeNormal Normal,int32 ShotId,UNGWeaponDefinition* ShotWeapon,bool bCharacter)
{
	if (GetNetMode()!=NM_DedicatedServer) { NGWeaponVFX::Impact(this,ShotWeapon,Point,Normal,bCharacter); OnProjectileImpact.Broadcast(Point,Normal,ShotId); }
}

void UNGCombatComponent::RequestReload()
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	auto* ASC = Cast<UNGAbilitySystemComponent>(GetCombatASC());
	if (!ASC || !NGPlayerContext::GetCurrentASC(Pawn, false) || ASC->HasMatchingGameplayTag(TAG_NGDead)
		|| ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f) return;
	ASC->TryActivateAbilityByInputTag(TAG_NGReloadInput);
}
bool UNGCombatComponent::CompleteReloadOnServer(UNGEquipmentInstance* Source)
{
	if (bCompletingReload || !Source || !Source->IsCurrent() || Source != GetWeaponInstance()) return false;
    auto* Item = Source->GetItem();
	const auto* Def = GetEquipment().Definition.Get();
	const auto* ASC = GetCombatASC();
	if (!ASC || !NGPlayerContext::GetCurrentASC(Cast<APawn>(GetOwner()), true)
		|| ASC->HasMatchingGameplayTag(TAG_NGDead) || ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f
		|| !GetEquipment().bEquipped || !Def || GetEquippedItem() != Item || !BoundInventory.IsValid() || !BoundInventory->Contains(Item)) return false;
	const int32 Amount = FMath::Min(FMath::Max(0, Def->MagazineCapacity - GetMagazineAmmo()), GetReserveAmmo());
	if (Amount <= 0) return false;
	UClass* Ammo=Def->AmmoItemClass.LoadSynchronous(); if (!Ammo || !Source->IsCurrent() || Source != GetWeaponInstance()) return false;
	bCompletingReload=true; ON_SCOPE_EXIT { bCompletingReload=false; };
	return BoundInventory->ConsumeItemsByDefinition(Ammo, Amount, [Item, Amount]
	{
		Item->AddStatTagStack(TAG_NGMagazineAmmo, Amount);
	});
}
void UNGCombatComponent::MulticastReload_Implementation(UNGWeaponDefinition* ReloadWeapon, int32 Revision)
{
    if (GetEquipment().Revision != Revision || GetEquipment().Definition != ReloadWeapon) return;
	if (GetNetMode() == NM_DedicatedServer) return;
	if (const auto* Def = GetEquipment().Definition.Get()) if (auto* Pawn = Cast<ACharacter>(GetOwner()))
	{
		if (Def->WeaponReloadAnimation && WeaponMesh.IsValid() && WeaponMesh->GetSkeletalMeshAsset()) WeaponMesh->PlayAnimation(Def->WeaponReloadAnimation, false);
		if (Def->CharacterReloadMontage) Pawn->PlayAnimMontage(Def->CharacterReloadMontage);
	}
}
