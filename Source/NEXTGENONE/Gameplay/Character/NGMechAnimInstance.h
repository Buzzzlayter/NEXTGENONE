#pragma once

#include "Animation/AnimInstance.h"
#include "NGMechAnimInstance.generated.h"

class ACharacter;
class UCharacterMovementComponent;
class UPrimitiveComponent;
class UPhysicsAsset;
class USkeletalMesh;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct FNGMechFootContact
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Mech|Grounding") FTransform Target = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly, Category="Mech|Grounding") float Weight = 0.f;
};

UCLASS(Blueprintable, meta=(PrioritizeCategories="Mech|Grounding"))
class NEXTGENONE_API UNGMechAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, Category="Mech|Grounding", meta=(ClampMin="1", Units="cm")) float StepHeight = 45.f;
	UPROPERTY(EditDefaultsOnly, Category="Mech|Grounding", meta=(ClampMin="1", Units="cm")) float MaxFootLockDistance = 45.f;
	UPROPERTY(EditDefaultsOnly, Category="Mech|Grounding", meta=(ClampMin="0", ClampMax="35", Units="deg")) float FootApproachPitch = 15.f;
	UPROPERTY(EditDefaultsOnly, Category="Mech|Grounding", meta=(ClampMin="0", ClampMax="35", Units="deg")) float FootPushOffPitch = 22.f;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Mech|Grounding") FNGMechFootContact LeftFoot;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Mech|Grounding") FNGMechFootContact RightFoot;

	UPROPERTY(EditDefaultsOnly, Category="Mech|Suspension", meta=(ClampMin="0.1", Units="Hz")) float SuspensionFrequency = 2.5f;
	UPROPERTY(EditDefaultsOnly, Category="Mech|Suspension", meta=(ClampMin="0.1")) float SuspensionDampingRatio = 0.75f;
	UPROPERTY(EditDefaultsOnly, Category="Mech|Suspension", meta=(ClampMin="0", Units="cm/s")) float StepCompressionImpulse = 180.f;
	UPROPERTY(EditDefaultsOnly, Category="Mech|Suspension", meta=(ClampMin="0", Units="cm")) float MaxSuspensionCompression = 20.f;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Mech|Suspension") FVector PelvisTranslation = FVector::ZeroVector;

private:
	struct FFootLock
	{
		TWeakObjectPtr<UPrimitiveComponent> Surface;
		FTransform RelativeTarget = FTransform::Identity;
	};
	struct FSolePoint
	{
		FVector FootLocal = FVector::ZeroVector;
		FVector MeshReference = FVector::ZeroVector;
		bool bValid = false;
	};
	struct FFootActuator
	{
		FTransform PreviousWorldTarget = FTransform::Identity;
		float Pitch = 0.f;
		float PitchVelocity = 0.f;
		float PreviousContact = 0.f;
		float SupportBlendTime = 0.f;
		bool bUnloading = false;
		bool bHasTarget = false;
		bool bStepArmed = false;
	};

	FFootLock LeftLock;
	FFootLock RightLock;
	FFootActuator LeftActuator;
	FFootActuator RightActuator;
	TWeakObjectPtr<USkeletalMesh> CalibratedMesh;
	TWeakObjectPtr<UPhysicsAsset> CalibratedPhysics;
	FSolePoint LeftSole[2];
	FSolePoint RightSole[2];
	FVector PreviousGroundVelocity = FVector::ZeroVector;
	FVector PreviousGroundFacing = FVector::ZeroVector;
	FVector BodyOffset = FVector::ZeroVector;
	FVector BodyOffsetVelocity = FVector::ZeroVector;
	float TerrainOffset = 0.f;
	float SuspensionOffset = 0.f;
	float SuspensionVelocity = 0.f;
	float FallingSpeed = 0.f;
	float BrakingLoad = 0.f;
	float TurningLoad = 0.f;
	bool bHasGroundVelocity = false;
	void RefreshSolePoints(const USkeletalMeshComponent& MeshComponent);
	static FSolePoint FindSolePoint(const USkeletalMesh& MeshAsset, const UPhysicsAsset* PhysicsAsset, FName Bone, FName Socket, const FTransform& FootReference);
	bool UpdateFoot(const ACharacter& Character, const UCharacterMovementComponent& Movement, const USkeletalMeshComponent& MeshComponent,
		FName JointBone, FName FootBone,
		const FSolePoint (&Sole)[2], double CapsuleBottom, float Contact, float DeltaSeconds, bool bTraveling, bool& bBrace,
		FFootLock& Lock, FFootActuator& Actuator, FNGMechFootContact& Foot, double& OutSupportHeight) const;
};
