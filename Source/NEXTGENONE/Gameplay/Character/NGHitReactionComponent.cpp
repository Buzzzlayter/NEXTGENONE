#include "Gameplay/Character/NGHitReactionComponent.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	bool HasPhysicalAnimationPose(USkeletalMeshComponent* Mesh)
	{
		if (!Mesh || !Mesh->GetSkeletalMeshAsset()) return false;
		const int32 BoneCount = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetNum();
		// This accessor completes parallel evaluation and returns the pose buffers to the mesh.
		// Read the editable component buffer afterwards: completing evaluation can swap it.
		const int32 LocalCount = Mesh->GetBoneSpaceTransformsView().Num();
		const int32 ComponentCount = Mesh->GetEditableComponentSpaceTransforms().Num();
		return BoneCount > 0 && LocalCount >= BoneCount && ComponentCount >= BoneCount;
	}
}

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
void UNGHitReactionComponent::EndPlay(EEndPlayReason::Type Reason) { PendingDeathImpact.Reset(); ReleaseBodies(IsRagdoll()); Super::EndPlay(Reason); }
void UNGHitReactionComponent::ApplyImpact(const FHitResult& Hit,FVector Direction,float Impulse)
{
	if (!GetOwner()->HasAuthority() || !FMath::IsFinite(Impulse) || Impulse<=0.f || Direction.ContainsNaN() || Hit.ImpactPoint.ContainsNaN()) return;
	const auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); auto* Mesh=Pawn ? Pawn->GetMesh() : nullptr;
	if (!Mesh || Hit.GetActor()!=Pawn || Direction.IsNearlyZero()) return;
	const FName Bone = Hit.GetComponent()==Mesh && !Hit.BoneName.IsNone() && Mesh->GetBodyInstance(Hit.BoneName) ? Hit.BoneName : TorsoFallbackBone;
	if (Bone.IsNone() || !Mesh->GetBodyInstance(Bone)) return;
	const FVector BonePoint = Mesh->GetSocketTransform(Bone).InverseTransformPosition(Hit.ImpactPoint);
	if (Pawn->IsDead() || Pawn->GetHealth()<=0.f)
		MulticastDeathImpact(Bone,BonePoint,Direction.GetSafeNormal(),FMath::Clamp(Impulse,0.f,10000.f));
	else MulticastImpact(Bone,BonePoint,Direction.GetSafeNormal(),FMath::Clamp(Impulse,0.f,10000.f));
}
void UNGHitReactionComponent::MulticastImpact_Implementation(FName Bone,FVector_NetQuantize BonePoint,FVector_NetQuantizeNormal Direction,float Impulse)
{
	ReceiveImpact(Bone,BonePoint,Direction,Impulse,false);
}
void UNGHitReactionComponent::MulticastDeathImpact_Implementation(FName Bone,FVector_NetQuantize BonePoint,FVector_NetQuantizeNormal Direction,float Impulse)
{
	ReceiveImpact(Bone,BonePoint,Direction,Impulse,true);
}
void UNGHitReactionComponent::ReleaseToRagdoll()
{
	ReleaseBodies(true);
	if (PendingDeathImpact.IsSet())
	{
		const auto Impact = PendingDeathImpact.GetValue();
		PendingDeathImpact.Reset();
		ApplyRagdollImpact(Impact.Bone,Impact.BonePoint,Impact.Direction,Impact.Impulse,true);
	}
}
void UNGHitReactionComponent::ApplyRagdollImpact(FName Bone,FVector BonePoint,FVector Direction,float Impulse,bool bFatal)
{
	const auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); auto* Mesh=Pawn ? Pawn->GetMesh() : nullptr;
	if (!Mesh || !IsRagdoll() || !Mesh->GetBodyInstance(Bone)) return;
	const FVector Point=Mesh->GetSocketTransform(Bone).TransformPosition(BonePoint);
	const float Multiplier=bFatal && FMath::IsFinite(DeathImpulseMultiplier) ? FMath::Clamp(DeathImpulseMultiplier,0.f,10.f) : 1.f;
	const FVector Force=Direction.GetSafeNormal()*Impulse*Multiplier;
	Mesh->WakeAllRigidBodies();
	if (bFatal && FMath::IsFinite(DeathPushSpeed))
		Mesh->AddImpulseToAllBodiesBelow(Direction.GetSafeNormal()*FMath::Clamp(DeathPushSpeed,0.f,600.f),NAME_None,true,true);
	Mesh->AddImpulseAtLocation(Force,Point,Bone);
	OnBulletBodyImpact.Broadcast(Bone,Point,Force);
}
void UNGHitReactionComponent::ReceiveImpact(FName Bone,FVector BonePoint,FVector Direction,float Impulse,bool bFatal)
{
	auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); auto* Mesh=Pawn ? Pawn->GetMesh() : nullptr;
	if (!Mesh || !Mesh->GetPhysicsAsset() || !Mesh->GetBodyInstance(Bone)) return;
	const FVector Point=Mesh->GetSocketTransform(Bone).TransformPosition(BonePoint),ImpulseVector=FVector(Direction)*FMath::Clamp(Impulse,0.f,10000.f);
	if (IsRagdoll())
	{
		ReleaseBodies(true); ApplyRagdollImpact(Bone,BonePoint,Direction,Impulse,bFatal); return;
	}
	if (bFatal)
	{
		// FSM/cue and this RPC may arrive in either order. Consume once after ALS enables physics.
		PendingDeathImpact = FPendingImpact{Bone,BonePoint,Direction,Impulse};
		return;
	}
	if (Pawn->IsDead() || PendingDeathImpact.IsSet()) return;
	// Damage is handled independently; never start drives against an unavailable animation pose.
	if (!HasPhysicalAnimationPose(Mesh)) return;
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
	auto* Mesh=GetSkeletalMesh(); if (!Mesh || ActiveBodies.IsEmpty()) return;
	// UE's physical-animation update indexes local/component transforms by skeleton bone index.
	// These buffers can be temporarily empty while animation/physics owns them during a transition.
	if (HasPhysicalAnimationPose(Mesh)) Super::TickComponent(Delta,Tick,ThisTick);
	// Recovery must continue even when drives cannot update this frame.
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
