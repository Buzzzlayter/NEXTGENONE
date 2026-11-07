#include "Gameplay/Combat/NGBallisticProjectile.h"
#include "Gameplay/Combat/NGCombat.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Character/NGHitReactionComponent.h"
#include "Gameplay/Combat/NGWeaponVFX.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ANGBallisticProjectile::ANGBallisticProjectile()
{
	bReplicates=true; SetReplicateMovement(true); PrimaryActorTick.bCanEverTick=true; SetNetUpdateFrequency(60.f); SetMinNetUpdateFrequency(30.f);
	Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision")); SetRootComponent(Collision); Collision->InitSphereRadius(.5f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Collision->SetCollisionObjectType(ECC_GameTraceChannel2);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore); Collision->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block); Collision->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block); Collision->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Block);
	Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tracer")); Visual->SetupAttachment(Collision); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetCastShadow(false);
	Movement=CreateDefaultSubobject<UNGBallisticMovement>(TEXT("Ballistics")); Movement->SetUpdatedComponent(Collision); Movement->bRotationFollowsVelocity=true; Movement->bInitialVelocityInLocalSpace=false;
	Movement->bForceSubStepping=true; Movement->MaxSimulationTimeStep=1.f/120.f; Movement->MaxSimulationIterations=16; Movement->ProjectileGravityScale=0.f;
	Movement->OnProjectileStop.AddDynamic(this,&ThisClass::OnStopped);
}
void ANGBallisticProjectile::Initialize(APawn* Shooter,UNGCombatComponent* Combat,FVector Velocity,FVector InGravity,float InDrag,float InDamage,float Range,int32 Shot,float ImpactImpulse)
{
	if (!IsValid(Shooter)) return;
	SourcePawn=Shooter; SourceASC=UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Shooter); SourceCombat=Combat;
	if (auto* ASC=Cast<UNGPlayerAbilitySystemComponent>(SourceASC.Get())) SourceLife=ASC->GetAvatarGeneration();
	InitialVelocity=Velocity; Gravity=InGravity; Drag=InDrag; Damage=InDamage; MaxRange=Range; ShotId=Shot;
	BodyImpulse=FMath::IsFinite(ImpactImpulse) ? FMath::Clamp(ImpactImpulse,0.f,10000.f) : 0.f;
	Collision->IgnoreActorWhenMoving(Shooter,true); TArray<AActor*> Attached; Shooter->GetAttachedActors(Attached); for (auto* Actor:Attached) Collision->IgnoreActorWhenMoving(Actor,true);
	OnRep_Ballistics(); SetLifeSpan(FMath::Clamp(Range/FMath::Max(1.f,Velocity.Size())*3.f,.1f,20.f));
}
void ANGBallisticProjectile::BeginPlay()
{
	Super::BeginPlay(); PreviousPosition=GetActorLocation();
	LastVFXPosition=GetActorLocation(); OnRep_Tracer();
	if (!HasAuthority()) Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bImpacted) OnRep_Impact();
}
void ANGBallisticProjectile::Tick(float Delta)
{
	Super::Tick(Delta);
	if (Tracer && !bImpacted && !GetActorLocation().Equals(LastVFXPosition,1.f))
	{
		bTracerTrigger=!bTracerTrigger; NGWeaponVFX::UpdateTracer(Tracer,LastVFXPosition,GetActorLocation(),bTracerTrigger); LastVFXPosition=GetActorLocation();
	}
	if (HasAuthority() && !bImpacted) { Travelled+=FVector::Distance(PreviousPosition,GetActorLocation()); PreviousPosition=GetActorLocation(); if (Travelled>=MaxRange) Destroy(); }
}
void ANGBallisticProjectile::OnRep_Ballistics() { if (!bImpacted) { Movement->Gravity=Gravity; Movement->Drag=Drag; Movement->Velocity=InitialVelocity; } }
void ANGBallisticProjectile::PostNetReceiveVelocity(const FVector& Velocity) { Super::PostNetReceiveVelocity(Velocity); if (!bImpacted) Movement->Velocity=Velocity; }
void ANGBallisticProjectile::OnRep_Impact() { if (bImpacted) { StopTracer(); Movement->StopMovementImmediately(); Movement->Deactivate(); Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetVisibility(false); } }
void ANGBallisticProjectile::SetWeaponEffects(UNGWeaponDefinition* Definition) { ShotWeapon=Definition; TracerFX=Definition ? Definition->TracerFX.Get() : nullptr; }
void ANGBallisticProjectile::OnRep_Tracer()
{
	if (!HasActorBegunPlay() || bImpacted || bTracerRequested || !TracerFX || GetNetMode()==NM_DedicatedServer) return;
	bTracerRequested=true; LastVFXPosition=GetActorLocation(); Tracer=NGWeaponVFX::CreateTracer(this,TracerFX,LastVFXPosition);
	if (Tracer) { Tracer->AddTickPrerequisiteActor(this); Visual->SetVisibility(false); }
}
void ANGBallisticProjectile::StopTracer() { if (Tracer) { NGWeaponVFX::StopTracer(Tracer); Tracer=nullptr; } }
void ANGBallisticProjectile::EndPlay(EEndPlayReason::Type Reason) { StopTracer(); Super::EndPlay(Reason); }
void ANGBallisticProjectile::OnStopped(const FHitResult& Hit)
{
	if (!HasAuthority() || bImpacted) return;
	bImpacted=true; OnRep_Impact();
	const auto* ASC = Cast<UNGPlayerAbilitySystemComponent>(SourceASC.Get());
	const auto* Shooter = SourcePawn.Get();
	if (Shooter && ASC && Shooter->GetPlayerState() == ASC->GetOwnerActor()
		&& ASC->GetAvatarGeneration()==SourceLife) if (auto* Target=Cast<APawn>(Hit.GetActor()))
	{
		if (UNGCombatComponent::ApplyDamageToPawn(Target,SourceASC.Get(),Damage,&Hit))
		{
			FVector Direction=(Hit.TraceEnd-Hit.TraceStart).GetSafeNormal(); if (Direction.IsNearlyZero()) Direction=GetActorForwardVector();
			if (auto* Reaction = Target->FindComponentByClass<UNGHitReactionComponent>()) Reaction->ApplyImpact(Hit,Direction,BodyImpulse);
		}
	}
	if (SourceCombat.IsValid()) SourceCombat->MulticastProjectileImpact(Hit.ImpactPoint,Hit.ImpactNormal,ShotId,ShotWeapon,Cast<ACharacter>(Hit.GetActor())!=nullptr);
	ForceNetUpdate(); SetLifeSpan(.3f);
}
void ANGBallisticProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(ANGBallisticProjectile,InitialVelocity,COND_InitialOnly); DOREPLIFETIME_CONDITION(ANGBallisticProjectile,Gravity,COND_InitialOnly); DOREPLIFETIME_CONDITION(ANGBallisticProjectile,Drag,COND_InitialOnly); DOREPLIFETIME(ANGBallisticProjectile,bImpacted);
	DOREPLIFETIME_CONDITION(ANGBallisticProjectile,TracerFX,COND_InitialOnly);
}
