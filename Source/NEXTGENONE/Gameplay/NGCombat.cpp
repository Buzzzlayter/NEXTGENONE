#include "NGCombat.h"
#include "NextgenonePlayerState.h"
#include "NextgenoneCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGFireInput, "Input.Combat.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGDamageAmount, "Data.Damage");

UNGCombatComponent::UNGCombatComponent() { SetIsReplicatedByDefault(true); }
void UNGCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner())) Pawn->OnDeathChanged.AddDynamic(this, &ThisClass::HandleDeath);
	OnRep_Equipment();
}
void UNGCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner())) Pawn->OnDeathChanged.RemoveDynamic(this, &ThisClass::HandleDeath);
	WeaponHandles.Remove(GrantedASC.Get());
	Super::EndPlay(Reason);
}
void UNGCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNGCombatComponent, Equipment);
	DOREPLIFETIME_CONDITION(UNGCombatComponent, MagazineAmmo, COND_OwnerOnly);
}
void UNGCombatComponent::RequestEquip(bool bEquip)
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled())) return;
	if (Pawn->HasAuthority()) SetEquippedOnServer(bEquip); else ServerSetEquipped(bEquip);
}
void UNGCombatComponent::ServerSetEquipped_Implementation(bool bEquip) { SetEquippedOnServer(bEquip); }
void UNGCombatComponent::SetEquippedOnServer(bool bEquip)
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority() || Equipment.bEquipped == bEquip) return;
	auto* ASC = Pawn->GetAbilitySystemComponent();
	if (bEquip && (!ASC || Pawn->IsDead() || !Pawn->GetController() || !TestWeapon)) return;
	WeaponHandles.Remove(GrantedASC.Get()); GrantedASC.Reset();
	Equipment.bEquipped = bEquip; ++Equipment.Revision;
	if (bEquip)
	{
		// A stable default subobject is network-addressable even without an asset pack.
		Equipment.Definition = TestWeapon;
		if (!bMagazineInitialized) { MagazineAmmo = FMath::Max(1, TestWeapon->MagazineCapacity); bMagazineInitialized = true; }
		if (!WeaponAbilities)
		{
			WeaponAbilities = NewObject<UNGAbilitySet>(this);
			FNGAbilityGrant Grant; Grant.Ability = UNGFireAbility::StaticClass(); Grant.InputTag = TAG_NGFireInput;
			WeaponAbilities->Abilities.Add(Grant);
		}
		WeaponAbilities->Give(ASC, this, WeaponHandles); GrantedASC = ASC;
	}
	OnRep_Equipment(); OnRep_Ammo(); Pawn->ForceNetUpdate();
}
void UNGCombatComponent::HandleDeath(bool bDead) { if (bDead && GetOwner()->HasAuthority()) SetEquippedOnServer(false); }
void UNGCombatComponent::RequestFire()
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	auto* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
	if (!ASC || Pawn->IsDead()) return;
	if (auto* Spec = ASC->FindAbilitySpecFromClass(UNGFireAbility::StaticClass())) ASC->TryActivateAbility(Spec->Handle);
}
bool UNGCombatComponent::FireOnServer()
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	auto* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
	auto* PS = Pawn ? Pawn->GetPlayerState<ANextgenonePlayerState>() : nullptr;
	const auto* Def = Equipment.Definition.Get();
	if (!Pawn || !Pawn->HasAuthority() || !ASC || !PS || !Def || !Equipment.bEquipped || Pawn->IsDead() || Pawn->GetHealth() <= 0 || !Pawn->GetController() || MagazineAmmo <= 0) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < PS->NextAllowedFireTime || !FMath::IsFinite(Def->FireInterval) || !FMath::IsFinite(Def->Damage) || !FMath::IsFinite(Def->Range) || Def->Damage <= 0 || Def->Range <= 0) return false;
	PS->NextAllowedFireTime = Now + FMath::Max(0.05f, Def->FireInterval);
	--MagazineAmmo; ++ShotSerial;
	FVector Eye; FRotator Aim; Pawn->GetActorEyesViewPoint(Eye, Aim);
	Aim = Pawn->GetController()->GetControlRotation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGWeapon), false, Pawn);
	TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult EyeHit;
	const FVector AimEnd = Eye + Aim.Vector() * FMath::Min(Def->Range, 100000.f);
	// Capsule hits for this slice. A server anchor avoids animation tick dependence.
	GetWorld()->LineTraceSingleByChannel(EyeHit, Eye, AimEnd, ECC_Visibility, Params);
	const FVector Start = Pawn->GetActorLocation() + FVector(0, 0, 45) + Pawn->GetActorForwardVector() * 30.f;
	const FVector End = EyeHit.bBlockingHit ? EyeHit.ImpactPoint : AimEnd;
	FHitResult Hit;
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End + (End-Start).GetSafeNormal()*2.f, ECC_Visibility, Params);
	if (auto* Target = Cast<ANextgenoneCharacter>(Hit.GetActor())) ApplyDamageToPawn(Target, ASC, Def->Damage, &Hit);
	MulticastShot(Start, Hit.bBlockingHit ? Hit.ImpactPoint : End, Hit.bBlockingHit, ShotSerial);
	OnRep_Ammo(); Pawn->ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("NG_SHOT pawn=%s ammo=%d hit=%s time=%.3f eye=%s aim=%s"), *Pawn->GetName(), MagazineAmmo, *GetNameSafe(Hit.GetActor()), Now, *Eye.ToCompactString(), *Aim.ToCompactString());
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
	Spec.Data->SetSetByCallerMagnitude(TAG_NGDamageAmount, Damage);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
	return true;
}
void UNGCombatComponent::OnRep_Equipment()
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (Pawn && GetNetMode() != NM_DedicatedServer)
	{
		if (!WeaponVisual)
		{
			WeaponVisual = NewObject<UStaticMeshComponent>(Pawn, TEXT("NGWeaponVisual"));
			WeaponVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision); WeaponVisual->RegisterComponent();
		}
		const auto* Def = Equipment.Definition ? Equipment.Definition.Get() : TestWeapon.Get();
		if (Def)
		{
			WeaponVisual->SetStaticMesh(Def->Mesh);
			WeaponVisual->AttachToComponent(Pawn->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Def->AttachSocket);
			WeaponVisual->SetRelativeTransform(Def->VisualTransform);
		}
		WeaponVisual->SetVisibility(Equipment.bEquipped);
	}
	OnEquipmentChanged.Broadcast(Equipment);
}
void UNGCombatComponent::OnRep_Ammo() { OnAmmoChanged.Broadcast(MagazineAmmo); }
void UNGCombatComponent::MulticastShot_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, int32 ShotId)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	OnShotCosmetic.Broadcast(Start, End, bHit, ShotId);
	if (FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke")))
		UE_LOG(LogTemp, Display, TEXT("NG_COSMETIC pawn=%s shot=%d hit=%d"), *GetOwner()->GetName(), ShotId, bHit);
	DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 0.3f, 0, 1.5f);
	if (bHit) DrawDebugSphere(GetWorld(), End, 8.f, 8, FColor::Red, false, 0.5f);
}
UNGFireAbility::UNGFireAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	ActivationBlockedTags.AddTag(TAG_NGDead);
}
void UNGFireAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Combat = Cast<UNGCombatComponent>(GetSourceObject(Handle, Info));
	if (Info && Info->IsNetAuthority() && Combat && Combat->GetOwner() == Info->AvatarActor.Get()) Combat->FireOnServer();
	EndAbility(Handle, Info, Activation, true, false);
}
