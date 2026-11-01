#include "NGHitReactionComponent.h"
#include "NextgenoneCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodyInstance.h"

UNGHitReactionComponent::UNGHitReactionComponent()
{
	SetIsReplicatedByDefault(true);
	Drive.bIsLocalSimulation=true; Drive.OrientationStrength=1200.f; Drive.AngularVelocityStrength=150.f; Drive.MaxAngularForce=50000.f;
}
void UNGHitReactionComponent::BeginPlay()
{
	Super::BeginPlay();
	auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); if (!Pawn) return;
	auto* Mesh=Pawn->GetMesh();
	// Bullets collide with Physics Asset bodies and return BoneName, never with the movement capsule.
	Pawn->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Ignore);
	Mesh->bEnablePhysicsOnDedicatedServer=true;
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Mesh->SetCollisionObjectType(ECC_Pawn); Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block);
	Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->bEnableUpdateRateOptimizations=false;
	if (!Mesh->IsPhysicsStateCreated()) Mesh->RecreatePhysicsState();
	SetSkeletalMeshComponent(Mesh);
}
bool UNGHitReactionComponent::IsRagdoll() const
{
	const auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); return Pawn && Pawn->GetMovementState()==EALSMovementState::Ragdoll;
}
void UNGHitReactionComponent::ReleaseBodies(bool bRagdollOwnsBodies)
{
	if (auto* Mesh=GetSkeletalMesh()) if (!bRagdollOwnsBodies)
	{
		for (const auto& State:ActiveBodies) if (auto* Body=Mesh->GetBodyInstance(State.Bone))
		{
			Mesh->SetBodySimulatePhysics(State.Bone,false); Body->PhysicsBlendWeight=State.OriginalBlend;
		}
		if (bSavedBlend) Mesh->bBlendPhysics=bOriginalBlend;
	}
	ActiveBodies.Reset(); bSavedBlend=false;
	SetSkeletalMeshComponent(nullptr); // Remove our drives without disabling an ALS ragdoll.
}
void UNGHitReactionComponent::EndPlay(EEndPlayReason::Type Reason) { ReleaseBodies(IsRagdoll()); Super::EndPlay(Reason); }
void UNGHitReactionComponent::ApplyBulletImpact(const FHitResult& Hit,FVector Direction,float Impulse)
{
	if (!GetOwner()->HasAuthority() || !FMath::IsFinite(Impulse) || Impulse<=0.f || Direction.ContainsNaN() || Hit.ImpactPoint.ContainsNaN()) return;
	const auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); auto* Mesh=Pawn ? Pawn->GetMesh() : nullptr;
	if (!Mesh || Hit.GetComponent()!=Mesh || Hit.BoneName.IsNone() || !Mesh->GetBodyInstance(Hit.BoneName)) return;
	MulticastImpact(Hit.BoneName,Mesh->GetSocketTransform(Hit.BoneName).InverseTransformPosition(Hit.ImpactPoint),Direction.GetSafeNormal(),FMath::Clamp(Impulse,0.f,10000.f));
}
void UNGHitReactionComponent::MulticastImpact_Implementation(FName Bone,FVector_NetQuantize BonePoint,FVector_NetQuantizeNormal Direction,float Impulse)
{
	auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); auto* Mesh=Pawn ? Pawn->GetMesh() : nullptr;
	if (!Mesh || !Mesh->GetPhysicsAsset() || !Mesh->GetBodyInstance(Bone)) return;
	const FVector Point=Mesh->GetSocketTransform(Bone).TransformPosition(BonePoint),ImpulseVector=FVector(Direction)*FMath::Clamp(Impulse,0.f,10000.f);
	if (IsRagdoll())
	{
		ReleaseBodies(true); Mesh->AddImpulseAtLocation(ImpulseVector,Point,Bone); OnBulletBodyImpact.Broadcast(Bone,Point,ImpulseVector); return;
	}
	if (!GetSkeletalMesh()) SetSkeletalMeshComponent(Mesh);
	FName Root=Bone;
	// Never simulate root/pelvis: the movement capsule must remain authoritative under ALS.
	if (Root==TEXT("pelvis") || Root==TEXT("root")) Root=TorsoFallbackBone;
	if (Root.IsNone() || Root==TEXT("pelvis") || Root==TEXT("root") || !Mesh->GetBodyInstance(Root)) return;
	if (!bSavedBlend) { bOriginalBlend=Mesh->bBlendPhysics; bSavedBlend=true; }
	Mesh->bBlendPhysics=false;
	const float Duration=FMath::IsFinite(RecoveryTime) ? FMath::Clamp(RecoveryTime,.05f,2.f) : .35f;
	for (const auto& Setup:Mesh->GetPhysicsAsset()->SkeletalBodySetups)
	{
		if (!Setup) continue; const FName Name=Setup->BoneName;
		if (Name!=Root && !Mesh->BoneIsChildOf(Name,Root)) continue;
		auto* Body=Mesh->GetBodyInstance(Name); if (!Body) continue;
		auto* State=ActiveBodies.FindByPredicate([&](const FBodyReaction& S){return S.Bone==Name;});
		if (!State)
		{
			if (Body->IsInstanceSimulatingPhysics()) continue; // Existing external physics is not ours to disable later.
			State=&ActiveBodies.AddDefaulted_GetRef(); State->Bone=Name; State->OriginalBlend=Body->PhysicsBlendWeight;
		}
		State->Remaining=Duration; ApplyPhysicalAnimationSettings(Name,Drive);
		Mesh->SetBodySimulatePhysics(Name,true); Body->PhysicsBlendWeight=FMath::Clamp(PeakBlend,0.f,1.f);
	}
	if (ActiveBodies.IsEmpty() && bSavedBlend) { Mesh->bBlendPhysics=bOriginalBlend; bSavedBlend=false; }
	Mesh->AddImpulseAtLocation(ImpulseVector,Point,Root);
	OnBulletBodyImpact.Broadcast(Bone,Point,ImpulseVector);
}
void UNGHitReactionComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* ThisTick)
{
	if (IsRagdoll() && !ActiveBodies.IsEmpty()) ReleaseBodies(true);
	Super::TickComponent(Delta,Tick,ThisTick);
	auto* Mesh=GetSkeletalMesh(); if (!Mesh || ActiveBodies.IsEmpty()) return;
	const float Duration=FMath::IsFinite(RecoveryTime) ? FMath::Clamp(RecoveryTime,.05f,2.f) : .35f;
	for (int32 I=ActiveBodies.Num()-1;I>=0;--I)
	{
		auto& State=ActiveBodies[I]; State.Remaining-=Delta;
		if (auto* Body=Mesh->GetBodyInstance(State.Bone))
		{
			Body->PhysicsBlendWeight=FMath::Lerp(State.OriginalBlend,FMath::Clamp(PeakBlend,0.f,1.f),FMath::Clamp(State.Remaining/Duration,0.f,1.f));
			if (State.Remaining<=0.f) { Mesh->SetBodySimulatePhysics(State.Bone,false); Body->PhysicsBlendWeight=State.OriginalBlend; ApplyPhysicalAnimationSettings(State.Bone,FPhysicalAnimationData()); }
		}
		if (State.Remaining<=0.f) ActiveBodies.RemoveAtSwap(I);
	}
	if (ActiveBodies.IsEmpty()) ReleaseBodies(false);
}
