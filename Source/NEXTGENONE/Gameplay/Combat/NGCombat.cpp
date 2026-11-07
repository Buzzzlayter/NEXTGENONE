#include "Gameplay/Combat/NGCombat.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Gameplay/Combat/NGBallisticProjectile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "Gameplay/Combat/NGWeaponVFX.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/Abilities/NGAimAbility.h"


UNGCombatComponent::UNGCombatComponent()
{
	SetIsReplicatedByDefault(true);
}
UNGWeaponDefinition* UNGCombatComponent::GetWeaponDefinition(const UNGInventoryItemDefinition* ItemDefinition)
{
	return ItemDefinition ? Cast<UNGWeaponDefinition>(ItemDefinition->GetEquipmentDefinition()) : nullptr;
}
void UNGCombatComponent::BindInventory()
{
	const auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	auto* PS = Pawn ? Pawn->GetPlayerState<ANextgenonePlayerState>() : nullptr;
	if (!PS || BoundInventory == PS->Inventory) return;
	if (BoundInventory.IsValid()) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::InventoryChanged);
	BoundInventory = PS->Inventory;
	PS->Inventory->OnInventoryChanged.AddDynamic(this, &ThisClass::InventoryChanged);
	if (Pawn->HasAuthority() && !StarterItems.IsEmpty() && PS->Inventory->InitialContents.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: migrate legacy Combat.StarterItems and reserve ammo to PlayerState.Inventory.InitialContents; legacy items are no longer granted."), *GetPathName());
	}

}
void UNGCombatComponent::InventoryChanged()
{
	if (GetOwner()->HasAuthority() && CurrentItem && BoundInventory.IsValid()
		&& (!BoundInventory->Contains(CurrentItem) || !BoundInventory->GetSlotTag(BoundInventory->GetSlotIndex(CurrentItem)).MatchesTag(TAG_NGSlotAction)))
	{
		SetEquippedOnServer(false); if (!BoundInventory->Contains(CurrentItem)) CurrentItem = nullptr; OnRep_CurrentItem();
	}
	OnRep_Ammo();
}
int32 UNGCombatComponent::GetMagazineAmmo() const { return CurrentItem ? CurrentItem->GetStatTagStackCount(TAG_NGMagazineAmmo) : 0; }
int32 UNGCombatComponent::GetReserveAmmo() const
{
	const auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); const auto* PS=Pawn ? Pawn->GetPlayerState<ANextgenonePlayerState>() : nullptr;
	const auto* Def=Equipment.Definition.Get(); UClass* Ammo=Def ? Def->AmmoItemClass.LoadSynchronous() : nullptr;
	return PS && Ammo ? PS->Inventory->GetTotalItemCountByDefinition(Ammo) : 0;
}
void UNGCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner())) Pawn->OnDeathChanged.AddDynamic(this, &ThisClass::HandleDeath);
	OnRep_Equipment();
}
void UNGCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopPrimaryInput();
	if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner())) Pawn->OnDeathChanged.RemoveDynamic(this, &ThisClass::HandleDeath);
	WeaponHandles.Remove(GrantedASC.Get());
	BindAbilitySystem(nullptr);
	if (BoundItem.IsValid()) BoundItem->OnStatsChanged.RemoveDynamic(this, &ThisClass::OnRep_Ammo);
	if (BoundInventory.IsValid()) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::InventoryChanged);
	Super::EndPlay(Reason);
}
void UNGCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNGCombatComponent, Equipment);
	DOREPLIFETIME_CONDITION(UNGCombatComponent, CurrentItem, COND_OwnerOnly);
}
void UNGCombatComponent::RequestEquip(bool bEquip)
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled())) return;
	if (bEquip)
	{
		const auto* Player = Pawn->GetPlayerState<ANextgenonePlayerState>();
		if (Player && CurrentItem) Player->Inventory->RequestActivationItem(CurrentItem);
	}
	else if (Pawn->HasAuthority()) SetEquippedOnServer(false); else ServerSetEquipped(false);
}
void UNGCombatComponent::ServerSetEquipped_Implementation(bool bEquip)
{
    if (!bEquip) SetEquippedOnServer(false);
}
void UNGCombatComponent::UnequipOnServer() { SetEquippedOnServer(false); }

void UNGCombatComponent::SetEquippedOnServer(bool bEquip)
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority() || Equipment.bEquipped == bEquip) return;
	auto* ASC = NGPlayerContext::GetCurrentASC(Pawn, true);
	if (bEquip && (!ASC || Pawn->IsDead() || !Pawn->GetController())) return;
	BindInventory();
	// Equipping never creates an item. Selection/pickup must provide an owned inventory instance.
	if (bEquip && (!BoundInventory.IsValid() || !BoundInventory->Contains(CurrentItem))) return;
	const auto* ItemDef = CurrentItem ? GetDefault<UNGInventoryItemDefinition>(CurrentItem->GetItemDef()) : nullptr;
	auto* Weapon = GetWeaponDefinition(ItemDef);
	FString WeaponError;
	if (bEquip && Weapon && !Weapon->ValidateDefinition(WeaponError))
	{ UE_LOG(LogTemp, Error, TEXT("Invalid weapon %s: %s"), *Weapon->GetName(), *WeaponError); return; }
	if (bEquip && (!Weapon || !BoundInventory->Contains(CurrentItem) || !BoundInventory->GetSlotTag(BoundInventory->GetSlotIndex(CurrentItem)).MatchesTag(TAG_NGSlotAction))) return;
	WeaponHandles.Remove(GrantedASC.Get()); GrantedASC.Reset();
	Equipment.bEquipped = bEquip; ++Equipment.Revision;
	if (CurrentItem) CurrentItem->SetEquipped(bEquip);
	if (bEquip)
	{
		Equipment.Definition = Weapon;
		FNGAbilityGrant Grant; Grant.Ability = Equipment.Definition->FireAbilityClass.LoadSynchronous();
		if (!Grant.Ability) Grant.Ability = UNGFireAbility::StaticClass();
		Grant.InputTag = TAG_NGFireInput;
		WeaponHandles.Abilities.Add(Grant.Give(ASC, this));
		Grant.Ability = Equipment.Definition->ReloadAbilityClass.LoadSynchronous();
		if (!Grant.Ability) Grant.Ability = UNGReloadAbility::StaticClass();
		Grant.InputTag = TAG_NGReloadInput;
		WeaponHandles.Abilities.Add(Grant.Give(ASC, this));
		GrantedASC = ASC;
		Grant.Ability = Equipment.Definition->AimAbilityClass;
		if (!Grant.Ability) Grant.Ability = UNGAimAbility::StaticClass();
		Grant.InputTag = TAG_NGInputAim;
		WeaponHandles.Abilities.Add(Grant.Give(ASC, this));
	}
	OnRep_CurrentItem(); OnRep_Equipment(); OnRep_Ammo(); Pawn->ForceNetUpdate();
}
bool UNGCombatComponent::EquipItemFromAbility(UNGInventoryItemInstance* Item)
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	BindInventory();
	if (!Pawn || !Pawn->HasAuthority() || Pawn->IsDead() || !NGPlayerContext::GetCurrentASC(Pawn, true) || !BoundInventory.IsValid() || !BoundInventory->Contains(Item)) return false;
	const auto* Def = GetDefault<UNGInventoryItemDefinition>(Item->GetItemDef());
	if (!GetWeaponDefinition(Def) || !BoundInventory->GetSlotTag(BoundInventory->GetSlotIndex(Item)).MatchesTag(TAG_NGSlotAction)) return false;
	if (Equipment.bEquipped && CurrentItem == Item) return true;
	SetEquippedOnServer(false); CurrentItem = Item; SetEquippedOnServer(true);
	return Equipment.bEquipped && CurrentItem == Item;
}
bool UNGCombatComponent::IsAiming() const
{
	const auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	const auto* ASC = ObservedASC.Get();
	return Equipment.bEquipped && Pawn && !Pawn->IsDead() && ASC && ASC->GetAvatarActor() == Pawn
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
	if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner())) Pawn->ApplyCombatPresentation();
	OnAimChanged.Broadcast(IsAiming());
}
void UNGCombatComponent::OnRep_CurrentItem()
{
	if (BoundItem.IsValid()) BoundItem->OnStatsChanged.RemoveDynamic(this, &ThisClass::OnRep_Ammo);
	BoundItem = CurrentItem;
	if (CurrentItem) CurrentItem->OnStatsChanged.AddDynamic(this, &ThisClass::OnRep_Ammo);
	OnRep_Ammo();
}
void UNGCombatComponent::HandleDeath(bool bDead) { if (bDead) { StopPrimaryInput(); if (GetOwner()->HasAuthority()) SetEquippedOnServer(false); } }

void UNGCombatComponent::StartPrimaryInput()
{
	StopPrimaryInput();
	const auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || Pawn->IsDead()) return;
	if (!Equipment.bEquipped)
	{
		if (auto* ASC = Cast<UNGAbilitySystemComponent>(Pawn->GetAbilitySystemComponent())) ASC->TryActivateAbilityByInputTag(TAG_NGInputPunch);
		return;
	}
	RequestFire();
	if (Equipment.Definition && Equipment.Definition->bAutomatic && FMath::IsFinite(Equipment.Definition->FireInterval))
		GetWorld()->GetTimerManager().SetTimer(PrimaryInputTimer, this, &ThisClass::ContinuePrimaryInput, FMath::Max(.05f, Equipment.Definition->FireInterval), true);
}

void UNGCombatComponent::StopPrimaryInput() { if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PrimaryInputTimer); }

void UNGCombatComponent::ContinuePrimaryInput()
{
	const auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || Pawn->IsDead() || !Equipment.bEquipped || !Equipment.Definition || !Equipment.Definition->bAutomatic)
	{ StopPrimaryInput(); return; }
	RequestFire();
}
void UNGCombatComponent::RequestFire()
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !IsAiming()) return;
	const auto* Aim = Pawn->FindComponentByClass<UNGAimComponent>();
	FVector Origin, Direction;
	if (!Aim || !Aim->GetViewRay(Origin, Direction)) return;
	if (Pawn->HasAuthority()) ServerRequestFire_Implementation(Origin, Direction, Equipment.Revision);
	else ServerRequestFire(Origin, Direction, Equipment.Revision);
}
void UNGCombatComponent::ServerRequestFire_Implementation(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection, int32 EquipmentRevision)
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	auto* ASC = Cast<UNGAbilitySystemComponent>(NGPlayerContext::GetCurrentASC(Pawn, true));
	const auto* Aim = Pawn ? Pawn->FindComponentByClass<UNGAimComponent>() : nullptr;
	if (!ASC || ASC->GetAvatarActor() != Pawn || Pawn->IsDead() || !Equipment.bEquipped
		|| !ASC->HasMatchingGameplayTag(TAG_NGWeaponAiming)
		|| EquipmentRevision != Equipment.Revision || !Aim || !Aim->ValidateShotView(ViewOrigin, ViewDirection)) return;
	TGuardValue<TOptional<FTransform>> ViewScope(RequestedShotView,
		TOptional<FTransform>(FTransform(FVector(ViewDirection).Rotation(), ViewOrigin)));
	ASC->TryActivateAbilityByInputTag(TAG_NGFireInput);
}
bool UNGCombatComponent::FireOnServer()
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	auto* ASC = Cast<UNGPlayerAbilitySystemComponent>(NGPlayerContext::GetCurrentASC(Pawn, true));
	auto* PS = Pawn ? Pawn->GetPlayerState<ANextgenonePlayerState>() : nullptr;
	const auto* Def = Equipment.Definition.Get();
	if (!Pawn || !Pawn->HasAuthority() || !ASC || !PS || !Def || !Equipment.bEquipped || Pawn->IsDead() || Pawn->GetHealth() <= 0 || !Pawn->GetController() || GetMagazineAmmo() <= 0 || !PS->Inventory->Contains(CurrentItem)) return false;
	FString WeaponError;
	if (!Def->ValidateDefinition(WeaponError)) return false;
	if (!ASC->HasMatchingGameplayTag(TAG_NGWeaponAiming) || ASC->HasMatchingGameplayTag(TAG_NGActivatingItem)) return false;
	if (ASC->HasMatchingGameplayTag(TAG_NGRagdollActive) || ASC->HasMatchingGameplayTag(TAG_NGRollActive)) return false;
	if (ASC->HasMatchingGameplayTag(TAG_NGDashActive) || ASC->HasMatchingGameplayTag(TAG_NGMantleActive) || ASC->HasMatchingGameplayTag(TAG_NGPunchActive) || ASC->HasMatchingGameplayTag(TAG_NGReloadActive)) return false;
	if (!ASC->CanFireNow(Pawn)) return false;
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
	const FVector Up=-Pawn->GetCharacterMovement()->GetGravityDirection();
	const FVector Start = Pawn->GetActorLocation() + Up*45.f + Pawn->GetActorForwardVector() * 30.f;
	const FVector End = EyeHit.bBlockingHit ? EyeHit.ImpactPoint : AimEnd;
	if (FVector::DotProduct(End - Start, ViewDirection) <= UE_SMALL_NUMBER) return false;
	FHitResult Obstruction;
	if (GetWorld()->LineTraceSingleByChannel(Obstruction,Pawn->GetActorLocation(),Start,ECC_Visibility,Params)) return false;
	if (!ASC->TryCommitFire(Pawn, Def->FireInterval)) return false;
	TArray<ANGBallisticProjectile*> Projectiles;
	const int32 Count=Def->PelletsPerShot;
	for (int32 I=0;I<Count;++I)
	{
		const FVector Direction=FMath::VRandCone((End-Start).GetSafeNormal(),FMath::DegreesToRadians(Def->SpreadDegrees));
		const FTransform Transform(Direction.Rotation(),Start);
		auto* Projectile=GetWorld()->SpawnActorDeferred<ANGBallisticProjectile>(Def->ProjectileClass,Transform,Pawn,Pawn,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile) { for (auto* Spawned:Projectiles) Spawned->Destroy(); return false; }
		Projectile->Initialize(Pawn,this,Direction*Def->MuzzleSpeed,-Up*FMath::Abs(GetWorld()->GetGravityZ())*Def->GravityScale,Def->LinearDrag,Def->Damage,Def->Range,ShotSerial+1,Def->BodyImpactImpulse);
		Projectile->SetWeaponEffects(Equipment.Definition);
		Projectiles.Add(Projectile);
	}
	CurrentItem->RemoveStatTagStack(TAG_NGMagazineAmmo, 1); ++ShotSerial;
	for (auto* Projectile:Projectiles) Projectile->FinishSpawning(Projectile->GetActorTransform());
	MulticastShot(Start,End,false,ShotSerial,Equipment.Definition);
	OnRep_Ammo(); Pawn->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("NG_BALLISTIC_SHOT pawn=%s ammo=%d pellets=%d"), *Pawn->GetName(),GetMagazineAmmo(),Count);
	return true;
}
bool UNGCombatComponent::ApplyDamageToPawn(ANextgenoneCharacter* Target, UAbilitySystemComponent* Source, float Damage, const FHitResult* Hit)
{
	if (!Target || !Target->HasAuthority() || Target->IsDead() || !Source || !Source->IsOwnerActorAuthoritative() || !FMath::IsFinite(Damage) || Damage <= 0) return false;
	auto* TargetASC = Target->GetAbilitySystemComponent();
	if (!TargetASC || TargetASC->GetAvatarActor() != Target) return false;
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
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (Pawn) Pawn->ApplyCombatPresentation();
	OnEquipmentChanged.Broadcast(Equipment);
	OnAimChanged.Broadcast(IsAiming());
}
void UNGCombatComponent::OnRep_Ammo() { OnAmmoChanged.Broadcast(GetMagazineAmmo()); }
void UNGCombatComponent::MulticastShot_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, int32 ShotId,UNGWeaponDefinition* ShotWeapon)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (const auto* Def = ShotWeapon) if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner()))
	{
		if (Def->WeaponFireAnimation && Pawn->SkeletalMesh->GetSkeletalMeshAsset()) Pawn->SkeletalMesh->PlayAnimation(Def->WeaponFireAnimation, false);
		if (Def->CharacterFireMontage) Pawn->PlayAnimMontage(Def->CharacterFireMontage);
		if (Def->FireSound) UGameplayStatics::PlaySoundAtLocation(this,Def->FireSound,Start);
		NGWeaponVFX::Shot(Pawn,Def,Start,(FVector(End)-FVector(Start)).GetSafeNormal());
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
	if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner()))
		if (auto* ASC = Cast<UNGAbilitySystemComponent>(Pawn->GetAbilitySystemComponent())) ASC->TryActivateAbilityByInputTag(TAG_NGReloadInput);
}
bool UNGCombatComponent::CompleteReloadOnServer(UNGInventoryItemInstance* Item)
{
	if (bCompletingReload) return false;
	const auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	const auto* Def = Equipment.Definition.Get();
	if (!Pawn || !Pawn->HasAuthority() || Pawn->IsDead() || !NGPlayerContext::GetCurrentASC(Pawn, true)
		|| !Equipment.bEquipped || !Def || CurrentItem != Item || !BoundInventory.IsValid() || !BoundInventory->Contains(Item)) return false;
	const int32 Amount = FMath::Min(FMath::Max(0, Def->MagazineCapacity - GetMagazineAmmo()), GetReserveAmmo());
	if (Amount <= 0) return false;
	UClass* Ammo=Def->AmmoItemClass.LoadSynchronous(); if (!Ammo) return false;
	bCompletingReload=true; ON_SCOPE_EXIT { bCompletingReload=false; };
	return BoundInventory->ConsumeItemsByDefinition(Ammo, Amount, [Item, Amount]
	{
		Item->AddStatTagStack(TAG_NGMagazineAmmo, Amount);
	});
}
void UNGCombatComponent::MulticastReload_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (const auto* Def = Equipment.Definition.Get()) if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner()))
	{
		if (Def->WeaponReloadAnimation && Pawn->SkeletalMesh->GetSkeletalMeshAsset()) Pawn->SkeletalMesh->PlayAnimation(Def->WeaponReloadAnimation, false);
		if (Def->CharacterReloadMontage) Pawn->PlayAnimMontage(Def->CharacterReloadMontage);
	}
}


void UNGCombatComponent::MulticastPunchStarted_Implementation(FVector_NetQuantizeNormal Direction, UAnimMontage* Montage)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (Montage) if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner())) Pawn->PlayAnimMontage(Montage);
	OnPunchStarted.Broadcast(Direction);
}
void UNGCombatComponent::MulticastPunchHit_Implementation(ANextgenoneCharacter* Target, FVector_NetQuantize ImpactPoint)
{
	if (GetNetMode() != NM_DedicatedServer) OnPunchHit.Broadcast(Target, ImpactPoint);
}
