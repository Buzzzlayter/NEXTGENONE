#include "Gameplay/Combat/NGRocketProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Field/FieldSystemObjects.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "Gameplay/Character/NGHitReactionComponent.h"
#include "Gameplay/Combat/NGDamage.h"
#include "GameplayEffect.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

bool FNGRocketDetonation::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint8 Occurred = bOccurred ? 1 : 0;
	Ar.SerializeBits(&Occurred, 1);
	if (Ar.IsLoading()) bOccurred = Occurred != 0;

	bool bPointSuccess = true;
	bool bNormalSuccess = true;
	if (bOccurred)
	{
		Point.NetSerialize(Ar, Map, bPointSuccess);
		Normal.NetSerialize(Ar, Map, bNormalSuccess);
	}
	bOutSuccess = bPointSuccess && bNormalSuccess && !Ar.IsError();
	return true;
}

ANGRocketProjectile::ANGRocketProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.f);
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(8.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_GameTraceChannel2);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
	Collision->SetGenerateOverlapEvents(false);

	RocketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RocketMesh"));
	RocketMesh->SetupAttachment(Collision);
	RocketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	Trail->SetupAttachment(RocketMesh);
	Trail->SetAutoActivate(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->ProjectileGravityScale = 0.f;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->bRotationFollowsVelocity = true;
	Movement->bInterpMovement = true;
	Movement->bInterpRotation = true;
	Movement->OnProjectileStop.AddDynamic(this, &ThisClass::OnProjectileStopped);
}

ANGAirburstRocketProjectile::ANGAirburstRocketProjectile()
{
	LaunchSpeed = 3600.f;
	FlightTime = 1.1f;
	bAirburstOnExpiry = true;
	BlastRadius = 600.f;
	DestructionStrain = 100000.f;
}

bool ANGRocketProjectile::ValidateSettings(FString& Error) const
{
	if (!GetIsReplicated() || !IsReplicatingMovement())
	{
		Error = TEXT("Rocket requires actor and movement replication.");
		return false;
	}
	const auto InRange = [](float Value, float Min, float Max)
	{
		return FMath::IsFinite(Value) && Value >= Min && Value <= Max;
	};
	if (!InRange(LaunchSpeed, 100.f, 40000.f)
		|| !InRange(FlightTime, 0.1f, 20.f) || !InRange(BlastRadius, 1.f, 10000.f)
		|| !InRange(EdgeDamageFraction, 0.f, 1.f) || !InRange(CharacterPushSpeed, 0.f, 5000.f)
		|| !InRange(PhysicsImpulse, 0.f, 10000000.f) || !InRange(DestructionStrain, 0.f, 100000000.f))
	{
		Error = TEXT("Rocket flight or blast settings are outside the supported range.");
		return false;
	}
	if (PayloadEffect && PayloadEffect->HasAnyClassFlags(CLASS_Abstract))
	{
		Error = TEXT("Rocket payload must be a concrete gameplay effect.");
		return false;
	}
	return true;
}

bool ANGRocketProjectile::Initialize(APawn* Shooter, const FVector& Direction, float Damage, float Range)
{
	auto* ASC = Cast<UNGPlayerAbilitySystemComponent>(NGDamage::FindLivingAbilitySystem(Shooter));
	if (!ASC || !ASC->IsOwnerActorAuthoritative()) return false;

	SourceASC = ASC;
	SourceGeneration = ASC->GetAvatarGeneration();
	BaseDamage = Damage;
	RemainingRange = Range;
	Movement->Velocity = Direction * LaunchSpeed;
	Movement->MaxSpeed = LaunchSpeed;
	Collision->IgnoreActorWhenMoving(Shooter, true);
	TArray<AActor*> Attached;
	Shooter->GetAttachedActors(Attached, true, true);
	for (auto* Actor : Attached) Collision->IgnoreActorWhenMoving(Actor, true);

	return true;
}

void ANGRocketProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		LastFlightPosition = GetActorLocation();
		AddTickPrerequisiteComponent(Movement);
		SetLifeSpan(FlightTime);
	}
	else
	{
		SetActorTickEnabled(false);
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Movement->bSimulationEnabled = false;
		Movement->SetInterpolatedComponent(RocketMesh);
	}
	if (Detonation.bOccurred) OnRep_Detonation();
	else if (GetNetMode() != NM_DedicatedServer && Trail->GetAsset()) Trail->Activate();
}

void ANGRocketProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || Detonation.bOccurred) return;

	const FVector Position = GetActorLocation();
	RemainingRange -= FVector::Dist(LastFlightPosition, Position);
	LastFlightPosition = Position;
	if (RemainingRange <= 0.f) LifeSpanExpired();
}

void ANGRocketProjectile::LifeSpanExpired()
{
	if (HasAuthority() && bAirburstOnExpiry && !Detonation.bOccurred)
	{
		Detonate(GetActorLocation(), FVector::UpVector, nullptr);
		return;
	}
	Super::LifeSpanExpired();
}

void ANGRocketProjectile::PostNetReceiveLocationAndRotation()
{
	if (!HasActorBegunPlay() || Detonation.bOccurred || !Collision->IsRegistered())
	{
		Super::PostNetReceiveLocationAndRotation();
		return;
	}
	const auto& Motion = GetReplicatedMovement();
	Movement->MoveInterpolationTarget(FRepMovement::RebaseOntoLocalOrigin(Motion.Location, this), Motion.Rotation);
}

void ANGRocketProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANGRocketProjectile, Detonation);
}

void ANGRocketProjectile::OnProjectileStopped(const FHitResult& Hit)
{
	if (HasAuthority() && Hit.bBlockingHit)
		Detonate(Hit.ImpactPoint, Hit.ImpactNormal, &Hit);
}

void ANGRocketProjectile::Detonate(const FVector& Point, const FVector& Normal, const FHitResult* DirectHit)
{
	if (Detonation.bOccurred) return;
	Detonation.Point = FRepMovement::RebaseOntoZeroOrigin(Point, this);
	Detonation.Normal = Normal;
	Detonation.bOccurred = true;
	OnRep_Detonation();
	const FVector BlastOrigin = Point + Normal * 2.f;
	ApplyBlast(BlastOrigin, DirectHit);
	ForceNetUpdate();
	SetLifeSpan(1.f);
}

void ANGRocketProjectile::OnRep_Detonation()
{
	if (!HasActorBegunPlay() || !Detonation.bOccurred) return;
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RocketMesh->SetVisibility(false);
	Trail->DeactivateImmediate();
	SetActorTickEnabled(false);
	if (GetNetMode() == NM_DedicatedServer) return;

	const FVector Point = FRepMovement::RebaseOntoLocalOrigin(Detonation.Point, this);
	auto* FX = ExplosionFX
		? UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionFX, Point, FVector(Detonation.Normal).Rotation(), FVector(ExplosionFXScale))
		: nullptr;
	if (FX)
	{
		FTimerHandle Cleanup;
		const float Lifetime = FMath::IsFinite(ExplosionFXMaxLifetime) ? FMath::Max(ExplosionFXMaxLifetime, 0.1f) : 5.f;
		GetWorldTimerManager().SetTimer(Cleanup, FTimerDelegate::CreateWeakLambda(FX, [FX]
		{
			FX->DestroyComponent();
		}), Lifetime, false);
	}
	if (ExplosionSound)
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, Point);
}

UNGPlayerAbilitySystemComponent* ANGRocketProjectile::GetCurrentSource() const
{
	auto* ASC = SourceASC.Get();
	const auto* Shooter = GetInstigator();
	return ASC && IsValid(Shooter) && ASC->GetAvatarActor() == Shooter
		&& ASC->GetAvatarGeneration() == SourceGeneration ? ASC : nullptr;
}

void ANGRocketProjectile::ApplyToCharacter(APawn* Target, const FVector& Origin, const FHitResult* DirectHit)
{
	auto* ASC = GetCurrentSource();
	if (!ASC) return;

	const FVector Offset = Target->GetActorLocation() - Origin;
	const float Falloff = DirectHit ? 1.f : 1.f - FMath::Clamp(static_cast<float>(Offset.Size()) / BlastRadius, 0.f, 1.f);
	const float Damage = BaseDamage * FMath::Lerp(EdgeDamageFraction, 1.f, Falloff);
	if (!NGDamage::ApplyToPawn(Target, ASC, Damage, DirectHit)) return;

	const FVector Direction = (Offset.GetSafeNormal() + FVector::UpVector * 0.25).GetSafeNormal();
	const float Push = CharacterPushSpeed * Falloff;
	if (auto* TargetASC = NGDamage::FindLivingAbilitySystem(Target))
	{
		if (PayloadEffect && GetCurrentSource() == ASC)
		{
			auto Context = ASC->MakeEffectContext();
			Context.AddSourceObject(this);
			if (DirectHit) Context.AddHitResult(*DirectHit);
			const auto Spec = ASC->MakeOutgoingSpec(PayloadEffect, 1.f, Context);
			if (Spec.IsValid()) ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		}
	}

	if (!IsValid(Target) || Push <= 0.f) return;
	if (NGDamage::FindLivingAbilitySystem(Target))
	{
		if (auto* Character = Cast<ACharacter>(Target))
		{
			Character->LaunchCharacter(Direction * Push, false, false);
			if (auto* CharacterMovement = Character->GetCharacterMovement())
				CharacterMovement->ForceClientAdjustment();
			Character->ForceNetUpdate();
		}
	}
	else if (auto* Reaction = Target->FindComponentByClass<UNGHitReactionComponent>())
	{
		const FHitResult BlastHit(Target, nullptr, Target->GetActorLocation(), -Direction);
		Reaction->ApplyImpact(DirectHit ? *DirectHit : BlastHit, Direction, Push);
	}
}

void ANGRocketProjectile::ApplyBlast(const FVector& Origin, const FHitResult* DirectHit)
{
	auto* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGRocketBlast), false, this);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams::AllObjects,
		FCollisionShape::MakeSphere(BlastRadius), Params);

	TSet<APawn*> HitPawns;
	TSet<UPrimitiveComponent*> HitComponents;
	for (const auto& Overlap : Overlaps)
	{
		auto* Component = Overlap.GetComponent();
		if (!IsValid(Component)) continue;
		if (auto* Pawn = Cast<APawn>(Overlap.GetActor())) HitPawns.Add(Pawn);
		else if (Component->IsSimulatingPhysics() || Component->IsA<UGeometryCollectionComponent>()) HitComponents.Add(Component);
	}
	const auto* DirectPawn = DirectHit ? Cast<APawn>(DirectHit->GetActor()) : nullptr;
	if (DirectHit)
	{
		if (auto* Pawn = Cast<APawn>(DirectHit->GetActor())) HitPawns.Add(Pawn);
		else if (auto* Component = DirectHit->GetComponent()) HitComponents.Add(Component);
	}

	auto IsExposed = [&](const AActor* Target, const FVector& Position)
	{
		FHitResult Obstruction;
		return !World->LineTraceSingleByChannel(Obstruction, Origin, Position, ECC_Visibility, Params)
			|| Obstruction.GetActor() == Target;
	};
	APawn* Shooter = GetInstigator();
	const bool bShooterInBlast = Shooter && HitPawns.Remove(Shooter) > 0;
	for (auto* Pawn : HitPawns)
	{
		if (!IsValid(Pawn)) continue;
		if (Pawn == DirectPawn) ApplyToCharacter(Pawn, Origin, DirectHit);
		else if (IsExposed(Pawn, Pawn->GetActorLocation())) ApplyToCharacter(Pawn, Origin, nullptr);
	}
	if (bShooterInBlast && IsValid(Shooter) && IsExposed(Shooter, Shooter->GetActorLocation()))
		ApplyToCharacter(Shooter, Origin, nullptr);

	URadialFalloff* StrainField = nullptr;
	for (auto* Component : HitComponents)
	{
		if (!IsValid(Component)) continue;
		const FVector NearestPoint = Component->Bounds.GetBox().GetClosestPointTo(Origin);
		if ((!DirectHit || Component != DirectHit->GetComponent()) && !IsExposed(Component->GetOwner(), NearestPoint)) continue;
		auto* Collection = Cast<UGeometryCollectionComponent>(Component);
		if (Collection && DestructionStrain > 0.f)
		{
			if (!StrainField)
				StrainField = NewObject<URadialFalloff>(this)->SetRadialFalloff(
					DestructionStrain, 0.f, 1.f, 0.f, BlastRadius, Origin, EFieldFalloffType::Field_Falloff_Linear);
			Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, StrainField);
		}
		if (PhysicsImpulse > 0.f && (Collection || Component->IsSimulatingPhysics()))
			Component->AddRadialImpulse(Origin, BlastRadius, PhysicsImpulse, RIF_Linear, false);
	}
}
