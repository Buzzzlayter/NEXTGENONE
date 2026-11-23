#include "Gameplay/Character/NGMechAnimInstance.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

namespace
{
	const FName LeftFootBone(TEXT("foot_l"));
	const FName RightFootBone(TEXT("foot_r"));
	const FName LeftToeBone(TEXT("ball_l"));
	const FName RightToeBone(TEXT("ball_r"));
	const FName LeftFootJoint(TEXT("calf_d_l"));
	const FName RightFootJoint(TEXT("calf_d_r"));
	const FName LeftHeelContact(TEXT("MechHeelContact_l"));
	const FName RightHeelContact(TEXT("MechHeelContact_r"));
	const FName LeftToeContact(TEXT("MechToeContact_l"));
	const FName RightToeContact(TEXT("MechToeContact_r"));
	const FName LeftPlantCurve(TEXT("MechFootPlantL"));
	const FName RightPlantCurve(TEXT("MechFootPlantR"));
	const FName PelvisBone(TEXT("Pelvis"));
	constexpr float FootBlendInSpeed = 16.f;
	constexpr float FootBlendOutSpeed = 11.f;
	constexpr double HardPenetrationDepth = 4.0;
	constexpr double SupportSwitchTolerance = 2.0;
	constexpr float SupportBlendDuration = 0.14f;

	float BlendFootWeight(float Current, float Target, float DeltaSeconds, float CollisionWeight = 0.f)
	{
		const float Speed = Target > Current ? FMath::Lerp(FootBlendInSpeed, 32.f, CollisionWeight) : FootBlendOutSpeed;
		const float Weight = FMath::Lerp(Current, Target, 1.f - FMath::Exp(-Speed * DeltaSeconds));
		return FMath::IsNearlyEqual(Weight, Target, 0.005f) ? Target : Weight;
	}
}

UNGMechAnimInstance::FSolePoint UNGMechAnimInstance::FindSolePoint(const USkeletalMesh& MeshAsset,
	const UPhysicsAsset* PhysicsAsset, FName Bone, FName Socket, const FTransform& FootReference)
{
	if (MeshAsset.GetRefSkeleton().FindBoneIndex(Bone) == INDEX_NONE) return {};
	const FTransform BoneReference(MeshAsset.GetComposedRefPoseMatrix(Bone));
	if (const auto* ContactSocket = MeshAsset.FindSocket(Socket))
	{
		if (MeshAsset.GetRefSkeleton().FindBoneIndex(ContactSocket->BoneName) != INDEX_NONE)
		{
			const FTransform SocketParent(MeshAsset.GetComposedRefPoseMatrix(ContactSocket->BoneName));
			const FVector Point = SocketParent.TransformPosition(ContactSocket->GetSocketLocalTransform().GetLocation());
			return {FootReference.InverseTransformPosition(Point), Point, true};
		}
	}

	const int32 BodyIndex = PhysicsAsset ? PhysicsAsset->FindBodyIndex(Bone) : INDEX_NONE;
	if (BodyIndex == INDEX_NONE || !PhysicsAsset->SkeletalBodySetups.IsValidIndex(BodyIndex)) return {};
	const auto* Body = PhysicsAsset->SkeletalBodySetups[BodyIndex].Get();
	if (!Body) return {};

	TArray<FVector, TInlineAllocator<64>> ReferenceVertices;
	double Bottom = TNumericLimits<double>::Max();
	for (const auto& Convex : Body->AggGeom.ConvexElems)
	{
		const FTransform ShapeReference = Convex.GetTransform() * BoneReference;
		for (const FVector& Vertex : Convex.VertexData)
		{
			const FVector Point = ShapeReference.TransformPosition(Vertex);
			ReferenceVertices.Add(Point);
			Bottom = FMath::Min(Bottom, Point.Z);
		}
	}
	FBox SoleBounds(ForceInit);
	for (const FVector& Point : ReferenceVertices)
		if (Point.Z <= Bottom + 0.5) SoleBounds += Point;
	if (SoleBounds.IsValid)
	{
		const FVector Point(SoleBounds.GetCenter().X, SoleBounds.GetCenter().Y, Bottom);
		return {FootReference.InverseTransformPosition(Point), Point, true};
	}

	const FBox ShapeBounds = Body->AggGeom.CalcAABB(BoneReference);
	if (!ShapeBounds.IsValid) return {};
	const FVector Point(ShapeBounds.GetCenter().X, ShapeBounds.GetCenter().Y, ShapeBounds.Min.Z);
	return {FootReference.InverseTransformPosition(Point), Point, true};
}

void UNGMechAnimInstance::RefreshSolePoints(const USkeletalMeshComponent& MeshComponent)
{
	auto* MeshAsset = MeshComponent.GetSkeletalMeshAsset();
	auto* PhysicsAsset = MeshComponent.GetPhysicsAsset();
	if (CalibratedMesh.Get() == MeshAsset && CalibratedPhysics.Get() == PhysicsAsset) return;
	CalibratedMesh = MeshAsset;
	CalibratedPhysics = PhysicsAsset;
	LeftLock = {};
	RightLock = {};
	LeftActuator = {};
	RightActuator = {};
	bHasGroundVelocity = false;
	BrakingLoad = 0.f;
	TurningLoad = 0.f;
	const auto CalibrateFoot = [MeshAsset, PhysicsAsset](FName FootBone, FName ToeBone, FName HeelSocket, FName ToeSocket, FSolePoint (&Sole)[2])
	{
		Sole[0] = {};
		Sole[1] = {};
		if (!MeshAsset || MeshAsset->GetRefSkeleton().FindBoneIndex(FootBone) == INDEX_NONE) return;

		const FTransform FootReference(MeshAsset->GetComposedRefPoseMatrix(FootBone));
		Sole[0] = FindSolePoint(*MeshAsset, PhysicsAsset, FootBone, HeelSocket, FootReference);
		Sole[1] = FindSolePoint(*MeshAsset, PhysicsAsset, ToeBone, ToeSocket, FootReference);
	};
	CalibrateFoot(LeftFootBone, LeftToeBone, LeftHeelContact, LeftToeContact, LeftSole);
	CalibrateFoot(RightFootBone, RightToeBone, RightHeelContact, RightToeContact, RightSole);
}

void UNGMechAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const auto* Character = Cast<ACharacter>(TryGetPawnOwner());
	const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const auto* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	auto* MeshComponent = GetSkelMeshComponent();
	const bool bCanUseGrounding = Movement && Capsule && MeshComponent && !MeshComponent->IsSimulatingPhysics();
	FVector Up = FVector::UpVector;
	double CapsuleBottom = 0.0;
	double RestOffset = 0.0;
	if (bCanUseGrounding)
	{
		RefreshSolePoints(*MeshComponent);
		Up = -Movement->GetGravityDirection();
		CapsuleBottom = FVector::DotProduct(Capsule->GetComponentLocation(), Up) - Capsule->GetScaledCapsuleHalfHeight();
		const FTransform& MeshTransform = MeshComponent->GetComponentTransform();
		double RestSoleHeight = TNumericLimits<double>::Max();
		for (const FSolePoint* Point : { &LeftSole[0], &LeftSole[1], &RightSole[0], &RightSole[1] })
		{
			if (Point->bValid)
				RestSoleHeight = FMath::Min(RestSoleHeight, FVector::DotProduct(MeshTransform.TransformPosition(Point->MeshReference), Up));
		}
		if (RestSoleHeight < TNumericLimits<double>::Max()) RestOffset = CapsuleBottom - RestSoleHeight;
	}
	if (!bCanUseGrounding || !Movement->IsMovingOnGround())
	{
		LeftLock = {};
		RightLock = {};
		LeftActuator = {};
		RightActuator = {};
		LeftFoot.Weight = bCanUseGrounding ? BlendFootWeight(LeftFoot.Weight, 0.f, DeltaSeconds) : 0.f;
		RightFoot.Weight = bCanUseGrounding ? BlendFootWeight(RightFoot.Weight, 0.f, DeltaSeconds) : 0.f;
		bHasGroundVelocity = false;
		BrakingLoad = 0.f;
		TurningLoad = 0.f;
		BodyOffset = FVector::ZeroVector;
		BodyOffsetVelocity = FVector::ZeroVector;
		SuspensionOffset = 0.f;
		SuspensionVelocity = 0.f;
		TerrainOffset = 0.f;
		PelvisTranslation = bCanUseGrounding ? MeshComponent->GetComponentTransform().InverseTransformVector(Up * RestOffset) : FVector::ZeroVector;
		FallingSpeed = bCanUseGrounding && Movement->IsFalling()
			? FMath::Max(0.f, static_cast<float>(FVector::DotProduct(Movement->Velocity, Movement->GetGravityDirection())))
			: 0.f;
		return;
	}

	const FVector GroundVelocity = Movement->ProjectToGravityFloor(Movement->Velocity);
	const double GroundSpeed = GroundVelocity.Size();
	const bool bMoving = GroundSpeed > 10.0;
	const FVector GroundFacing = Movement->ProjectToGravityFloor(Character->GetActorForwardVector()).GetSafeNormal();
	const bool bHasMotionSample = bHasGroundVelocity && DeltaSeconds > UE_SMALL_NUMBER && DeltaSeconds <= 0.15f;
	const FVector Acceleration = bHasMotionSample ? (GroundVelocity - PreviousGroundVelocity) / DeltaSeconds : FVector::ZeroVector;
	const double PreviousSpeed = PreviousGroundVelocity.Size();
	const double SpeedLoss = bHasMotionSample && PreviousSpeed > 65.0 ? (PreviousSpeed - GroundSpeed) / DeltaSeconds : 0.0;
	const double FacingChange = bHasMotionSample && !GroundFacing.IsNearlyZero() && !PreviousGroundFacing.IsNearlyZero()
		? FMath::Acos(FMath::Clamp(FVector::DotProduct(PreviousGroundFacing, GroundFacing), -1.0, 1.0)) : 0.0;
	const double TurnRate = bHasMotionSample && FacingChange < UE_PI / 3.0 ? FacingChange / DeltaSeconds : 0.0;
	const float BrakeTarget = static_cast<float>(FMath::Clamp((SpeedLoss - 90.0) / 650.0, 0.0, 1.0));
	const float TurnTarget = static_cast<float>(FMath::Clamp((TurnRate - 0.7) / 2.8, 0.0, 1.0));
	BrakingLoad = FMath::Max(BrakeTarget, FMath::FInterpTo(BrakingLoad, 0.f, DeltaSeconds, 6.f));
	TurningLoad = FMath::FInterpTo(TurningLoad, TurnTarget, DeltaSeconds, TurnTarget > TurningLoad ? 14.f : 7.f);
	PreviousGroundVelocity = GroundVelocity;
	PreviousGroundFacing = GroundFacing;
	bHasGroundVelocity = true;

	float LeftContact;
	float RightContact;
	GetCurveValueWithDefault(LeftPlantCurve, bMoving ? 0.f : 1.f, LeftContact);
	GetCurveValueWithDefault(RightPlantCurve, bMoving ? 0.f : 1.f, RightContact);
	const bool bCanBraceLeft = LeftLock.Surface.IsValid();
	const bool bCanBraceRight = RightLock.Surface.IsValid();
	const bool bWantsBrace = FMath::Max(BrakingLoad, TurningLoad) > 0.2f;
	bool bBraceLeft = bWantsBrace && bCanBraceLeft && (!bCanBraceRight || LeftFoot.Weight >= RightFoot.Weight);
	bool bBraceRight = bWantsBrace && bCanBraceRight && !bBraceLeft;
	double LeftSupport = 0.0;
	double RightSupport = 0.0;
	const bool bLeftContact = UpdateFoot(*Character, *Movement, *MeshComponent, LeftFootJoint, LeftFootBone,
		LeftSole, CapsuleBottom, LeftContact, DeltaSeconds, bMoving, bBraceLeft, LeftLock, LeftActuator, LeftFoot, LeftSupport);
	const bool bRightContact = UpdateFoot(*Character, *Movement, *MeshComponent, RightFootJoint, RightFootBone,
		RightSole, CapsuleBottom, RightContact, DeltaSeconds, bMoving, bBraceRight, RightLock, RightActuator, RightFoot, RightSupport);

	const double SupportHeight = bLeftContact && bRightContact ? FMath::Min(LeftSupport, RightSupport)
		: bLeftContact ? LeftSupport : bRightContact ? RightSupport : CapsuleBottom;
	const float TargetTerrainOffset = static_cast<float>(FMath::Clamp(SupportHeight - CapsuleBottom, -static_cast<double>(StepHeight), 0.0));
	TerrainOffset = FMath::FInterpTo(TerrainOffset, TargetTerrainOffset, DeltaSeconds, 8.f);
	const auto ConsumeStep = [bMoving](FFootActuator& Actuator, float PlantCurve, bool bOnSupport)
	{
		if (!bMoving)
		{
			Actuator.bStepArmed = false;
			return false;
		}
		if (!bOnSupport && PlantCurve <= 0.25f)
			Actuator.bStepArmed = true;
		if (!bOnSupport || PlantCurve < 0.55f || !Actuator.bStepArmed)
			return false;

		Actuator.bStepArmed = false;
		return true;
	};
	const bool bLeftStep = ConsumeStep(LeftActuator, LeftContact, bLeftContact);
	const bool bRightStep = ConsumeStep(RightActuator, RightContact, bRightContact);
	if (FallingSpeed > 0.f)
		SuspensionVelocity -= FMath::Min(FallingSpeed * 0.65f, 650.f);
	else if (bLeftStep || bRightStep)
		SuspensionVelocity -= StepCompressionImpulse;
	FallingSpeed = 0.f;
	const bool bBraced = (bBraceLeft && bLeftContact) || (bBraceRight && bRightContact);
	const float BraceLoad = bBraced ? FMath::Max(BrakingLoad, TurningLoad * 0.55f) : 0.f;
	const float CompressionTarget = -MaxSuspensionCompression * 0.4f * BraceLoad;
	FMath::SpringDamper(SuspensionOffset, SuspensionVelocity, CompressionTarget, 0.f, DeltaSeconds, SuspensionFrequency, SuspensionDampingRatio);
	if (SuspensionOffset < -MaxSuspensionCompression)
	{
		SuspensionOffset = -MaxSuspensionCompression;
		SuspensionVelocity = FMath::Max(0.f, SuspensionVelocity);
	}
	const FTransform& MeshTransform = MeshComponent->GetComponentTransform();
	FVector SupportShift = FVector::ZeroVector;
	const float LeftLoad = bLeftContact ? LeftFoot.Weight : 0.f;
	const float RightLoad = bRightContact ? RightFoot.Weight : 0.f;
	const float TotalLoad = LeftLoad + RightLoad;
	if (TotalLoad > UE_KINDA_SMALL_NUMBER)
	{
		const FVector PelvisPosition = MeshComponent->GetSocketLocation(PelvisBone) - BodyOffset;
		FVector CenterOfSupport = (LeftFoot.Target.GetLocation() * LeftLoad + RightFoot.Target.GetLocation() * RightLoad) / TotalLoad;
		if (bBraced)
		{
			const FVector BraceTarget = bBraceLeft ? LeftFoot.Target.GetLocation() : RightFoot.Target.GetLocation();
			CenterOfSupport = FMath::Lerp(CenterOfSupport, BraceTarget, BraceLoad * 0.65f);
		}
		SupportShift = Movement->ProjectToGravityFloor(MeshTransform.TransformPosition(CenterOfSupport) - PelvisPosition)
			.GetClampedToMaxSize(55.0) * 0.18;
	}
	const FVector InertialShift = (Acceleration * -0.012).GetClampedToMaxSize(12.0);
	const FVector TargetBodyOffset = SupportShift + InertialShift;
	FMath::SpringDamper(BodyOffset, BodyOffsetVelocity, TargetBodyOffset, FVector::ZeroVector, DeltaSeconds, 2.2f, 0.9f);
	PelvisTranslation = MeshTransform.InverseTransformVector(Up * (RestOffset + TerrainOffset + SuspensionOffset) + BodyOffset);
}

bool UNGMechAnimInstance::UpdateFoot(const ACharacter& Character, const UCharacterMovementComponent& Movement,
	const USkeletalMeshComponent& MeshComponent, FName JointBone, FName FootBone,
	const FSolePoint (&Sole)[2], double CapsuleBottom, float Contact, float DeltaSeconds, bool bTraveling, bool& bBrace,
	FFootLock& Lock, FFootActuator& Actuator, FNGMechFootContact& Foot, double& OutSupportHeight) const
{
	const auto* World = Character.GetWorld();
	if (!World || MeshComponent.GetBoneIndex(JointBone) == INDEX_NONE || (!Sole[0].bValid && !Sole[1].bValid))
	{
		Lock = {};
		Actuator = {};
		Foot.Weight = 0.f;
		bBrace = false;
		return false;
	}

	const FVector Up = -Movement.GetGravityDirection();
	const FTransform Joint = MeshComponent.GetSocketTransform(JointBone);
	FTransform WorldTarget(Joint.GetRotation(), Joint.GetLocation());
	if (const auto* Surface = Lock.Surface.Get())
	{
		const FTransform LockedTarget = Lock.RelativeTarget * Surface->GetComponentTransform();
		if (FVector::DistSquared(LockedTarget.GetLocation(), Joint.GetLocation()) <= FMath::Square(MaxFootLockDistance))
			WorldTarget = LockedTarget;
		else
			Lock = {};
	}
	bBrace = bBrace && Lock.Surface.IsValid();
	const float DesiredWeight = FMath::Clamp(bBrace ? FMath::Max(Contact, 0.85f) : Contact, 0.f, 1.f);
	if (!bTraveling || DesiredWeight <= 0.1f || DesiredWeight >= 0.92f || DesiredWeight > Actuator.PreviousContact + 0.01f)
		Actuator.bUnloading = false;
	else if (DesiredWeight < Actuator.PreviousContact - 0.01f)
		Actuator.bUnloading = true;
	Actuator.PreviousContact = DesiredWeight;
	const bool bPlanting = DesiredWeight >= (Lock.Surface.IsValid() ? 0.25f : 0.55f);
	if (!bPlanting)
	{
		Lock = {};
		WorldTarget = FTransform(Joint.GetRotation(), Joint.GetLocation());
	}

	const FTransform FootTransform = MeshComponent.GetSocketTransform(FootBone);
	const FVector SolePoints[] = {FootTransform.TransformPosition(Sole[0].FootLocal), FootTransform.TransformPosition(Sole[1].FootLocal)};
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MechFootContact), false, &Character);
	FCollisionObjectQueryParams SurfaceTypes;
	SurfaceTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	SurfaceTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult GroundHits[UE_ARRAY_COUNT(SolePoints)];
	const FHitResult* Support = nullptr;
	const FHitResult* LockedSupport = nullptr;
	double HighestLift = -TNumericLimits<double>::Max();
	double LockedLift = -TNumericLimits<double>::Max();
	const FQuat TargetRotation = WorldTarget.GetRotation() * Joint.GetRotation().Inverse();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SolePoints); ++Index)
	{
		if (!Sole[Index].bValid) continue;
		const FVector TargetSole = WorldTarget.GetLocation() + TargetRotation.RotateVector(SolePoints[Index] - Joint.GetLocation());
		const double SoleHeight = FVector::DotProduct(TargetSole, Up);
		const double StartOffset = StepHeight + FMath::Max(0.0, CapsuleBottom - SoleHeight);
		const double EndOffset = StepHeight + FMath::Max(0.0, SoleHeight - CapsuleBottom);
		auto& Hit = GroundHits[Index];
		if (!World->LineTraceSingleByObjectType(Hit, TargetSole + Up * StartOffset, TargetSole - Up * EndOffset, SurfaceTypes, Query)
			|| !Hit.IsValidBlockingHit() || !Movement.IsWalkable(Hit) || !Hit.GetComponent())
		{
			Hit = FHitResult{};
			continue;
		}
		const double Lift = FVector::DotProduct(Hit.ImpactPoint - TargetSole, Up);
		if (Hit.GetComponent() == Lock.Surface.Get() && Lift > LockedLift)
		{
			LockedSupport = &Hit;
			LockedLift = Lift;
		}
		if (Lift <= HighestLift) continue;
		Support = &Hit;
		HighestLift = Lift;
	}
	if (LockedSupport && HighestLift - LockedLift <= SupportSwitchTolerance) Support = LockedSupport;
	if (!Support)
	{
		Lock = {};
		Actuator.SupportBlendTime = 0.f;
		bBrace = false;
		FMath::SpringDamper(Actuator.Pitch, Actuator.PitchVelocity, 0.f, 0.f, DeltaSeconds, 5.f, 0.85f);
		Foot.Weight = BlendFootWeight(Foot.Weight, 0.f, DeltaSeconds);
		return false;
	}

	bBrace = bBrace && Lock.Surface.Get() == Support->GetComponent();
	if (Lock.Surface.Get() != Support->GetComponent())
	{
		if (bPlanting && Actuator.bHasTarget && Foot.Weight > 0.f)
			Actuator.SupportBlendTime = SupportBlendDuration;
		const FQuat GroundRotation = FQuat::FindBetweenNormals(Up, Support->ImpactNormal) * Joint.GetRotation();
		const FQuat PreviousRotation = Foot.Weight > 0.f
			? MeshComponent.GetComponentTransform().TransformRotation(Foot.Target.GetRotation()) : Joint.GetRotation();
		WorldTarget.SetRotation(FQuat::Slerp(PreviousRotation, GroundRotation, 1.f - FMath::Exp(-18.f * DeltaSeconds)).GetNormalized());
	}
	FTransform PlantTarget = WorldTarget;
	const bool bHasRockerGeometry = Sole[0].bValid && Sole[1].bValid;
	const float ApproachWeight = static_cast<float>(FMath::Clamp(1.0 + HighestLift / 24.0, 0.0, 1.0));
	float DesiredPitch = 0.f;
	if (bTraveling && bHasRockerGeometry)
	{
		if (Actuator.bUnloading)
			DesiredPitch = -FootPushOffPitch * FMath::Clamp((0.9f - DesiredWeight) / 0.55f, 0.f, 1.f);
		else if (!bPlanting)
			DesiredPitch = FootApproachPitch * ApproachWeight;
	}
	FMath::SpringDamper(Actuator.Pitch, Actuator.PitchVelocity, DesiredPitch, 0.f, DeltaSeconds, 5.f, 0.85f);
	bool bRockerActive = false;
	if (bHasRockerGeometry)
	{
		const FVector Normal = Support->ImpactNormal;
		const FVector SoleDirection = (PlantTarget.GetRotation() * Joint.GetRotation().Inverse())
			.RotateVector(SolePoints[1] - SolePoints[0]).GetSafeNormal();
		const FVector Hinge = FVector::CrossProduct(SoleDirection, Normal).GetSafeNormal();
		if (!Hinge.IsNearlyZero())
		{
			bRockerActive = FMath::Abs(Actuator.Pitch) > 0.1f;
			if (bPlanting || bRockerActive)
			{
				const double SlopePitch = FMath::Asin(FMath::Clamp(FVector::DotProduct(SoleDirection, Normal), -1.0, 1.0));
				const FQuat GroundAlignment(Hinge, -FMath::Clamp(SlopePitch, -0.45, 0.45));
				PlantTarget.SetRotation((GroundAlignment * PlantTarget.GetRotation()).GetNormalized());
				WorldTarget = PlantTarget;
			}
			if (bRockerActive)
			{
				const FQuat Rocker(Hinge, FMath::DegreesToRadians(static_cast<double>(Actuator.Pitch)));
				const int32 PivotIndex = Actuator.Pitch > 0.f ? 0 : 1;
				const FQuat GroundRotation = PlantTarget.GetRotation() * Joint.GetRotation().Inverse();
				const FVector Pivot = GroundRotation.RotateVector(SolePoints[PivotIndex] - Joint.GetLocation());
				WorldTarget.AddToTranslation(Pivot - Rocker.RotateVector(Pivot));
				WorldTarget.SetRotation((Rocker * PlantTarget.GetRotation()).GetNormalized());
			}
		}
	}
	const FQuat NeutralRotation = PlantTarget.GetRotation() * Joint.GetRotation().Inverse();
	const FQuat SoleRotation = WorldTarget.GetRotation() * Joint.GetRotation().Inverse();
	double TargetLift = -TNumericLimits<double>::Max();
	double PlantLift = -TNumericLimits<double>::Max();
	double PenetrationDepth = 0.0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SolePoints); ++Index)
	{
		const auto& Hit = GroundHits[Index];
		if (!Hit.bBlockingHit) continue;
		const FVector Normal = Hit.ImpactNormal;
		const FVector AlignedSole = WorldTarget.GetLocation() + SoleRotation.RotateVector(SolePoints[Index] - Joint.GetLocation());
		const FVector NeutralSole = PlantTarget.GetLocation() + NeutralRotation.RotateVector(SolePoints[Index] - Joint.GetLocation());
		const double UpDot = FMath::Max(0.1, FVector::DotProduct(Up, Normal));
		const double PlaneLift = FVector::DotProduct(Hit.ImpactPoint - AlignedSole, Normal) / UpDot;
		TargetLift = FMath::Max(TargetLift, PlaneLift);
		PlantLift = FMath::Max(PlantLift, FVector::DotProduct(Hit.ImpactPoint - NeutralSole, Normal) / UpDot);
		PenetrationDepth = FMath::Max(PenetrationDepth, FVector::DotProduct(Hit.ImpactPoint - SolePoints[Index], Normal));
	}
	if (!bPlanting && !bRockerActive && PenetrationDepth <= 0.5)
	{
		Foot.Weight = BlendFootWeight(Foot.Weight, 0.f, DeltaSeconds);
		return false;
	}
	if (Lock.Surface.Get() == Support->GetComponent())
	{
		const double SettleAlpha = 1.0 - FMath::Exp(-12.0 * DeltaSeconds);
		if (TargetLift < 0.0) TargetLift *= SettleAlpha;
		if (PlantLift < 0.0) PlantLift *= SettleAlpha;
	}
	if (!bPlanting) TargetLift = FMath::Max(0.0, TargetLift);
	WorldTarget.AddToTranslation(Up * TargetLift);
	if (bPlanting)
	{
		Lock.Surface = Support->GetComponent();
		PlantTarget.AddToTranslation(Up * PlantLift);
		Lock.RelativeTarget = PlantTarget.GetRelativeTransform(Support->GetComponent()->GetComponentTransform());
	}

	if (Actuator.SupportBlendTime > DeltaSeconds && bPlanting)
	{
		Actuator.SupportBlendTime -= DeltaSeconds;
		Actuator.PreviousWorldTarget.BlendWith(WorldTarget, 1.f - FMath::Exp(-28.f * DeltaSeconds));
	}
	else
	{
		Actuator.SupportBlendTime = 0.f;
		Actuator.PreviousWorldTarget = WorldTarget;
	}
	Actuator.bHasTarget = true;
	Foot.Target = Actuator.PreviousWorldTarget.GetRelativeTransform(MeshComponent.GetComponentTransform());
	const float CollisionWeight = static_cast<float>(FMath::Clamp((PenetrationDepth - 0.5) / (HardPenetrationDepth - 0.5), 0.0, 1.0));
	const float ActuatorWeight = bRockerActive ? FMath::Clamp(FMath::Abs(Actuator.Pitch) / 8.f, 0.f, 1.f) * 0.85f : 0.f;
	Foot.Weight = BlendFootWeight(Foot.Weight, FMath::Max3(DesiredWeight, CollisionWeight, ActuatorWeight), DeltaSeconds, CollisionWeight);
	OutSupportHeight = FVector::DotProduct(Support->ImpactPoint, Up);
	return bPlanting;
}
