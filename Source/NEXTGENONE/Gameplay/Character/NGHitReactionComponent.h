#pragma once
#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "NGHitReactionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNGBulletBodyImpact,FName,Bone,FVector,Point,FVector,Impulse);

/** Short physical-animation reaction; ALS retains capsule locomotion and ragdoll ownership. */
UCLASS(ClassGroup=Gameplay,meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGHitReactionComponent : public UPhysicalAnimationComponent
{
	GENERATED_BODY()
public:
	UNGHitReactionComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* ThisTick) override;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Hit Reaction",meta=(ClampMin="0.05",ClampMax="2")) float RecoveryTime=.35f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Hit Reaction",meta=(ClampMin="0",ClampMax="1")) float PeakBlend=.8f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Hit Reaction") FName TorsoFallbackBone=TEXT("spine_01");
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Hit Reaction") FPhysicalAnimationData Drive;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Hit Reaction|Death",meta=(ClampMin="0",ClampMax="10")) float DeathImpulseMultiplier=2.5f;
	/** Whole-body speed in the attack direction, cm/s; no artificial upward bias. */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Hit Reaction|Death",meta=(ClampMin="0",ClampMax="600")) float DeathPushSpeed=100.f;
	UPROPERTY(BlueprintAssignable,Category="Hit Reaction") FNGBulletBodyImpact OnBulletBodyImpact;
	void ApplyImpact(const FHitResult& Hit,FVector Direction,float Impulse);
	void ReleaseToRagdoll();
private:
	struct FPendingImpact { FName Bone; FVector BonePoint; FVector Direction; float Impulse; };
	TOptional<FPendingImpact> PendingDeathImpact;
	struct FBodyReaction { FName Bone; float Remaining=0.f; float OriginalBlend=0.f; };
	TArray<FBodyReaction> ActiveBodies;
	bool bSavedBlend=false;
	bool bOriginalBlend=false;
	bool IsRagdoll() const;
	void ReleaseBodies(bool bRagdollOwnsBodies);
	void ReceiveImpact(FName Bone,FVector BonePoint,FVector Direction,float Impulse,bool bFatal);
	void ApplyRagdollImpact(FName Bone,FVector BonePoint,FVector Direction,float Impulse,bool bFatal);
	UFUNCTION(NetMulticast,Unreliable) void MulticastImpact(FName Bone,FVector_NetQuantize BonePoint,FVector_NetQuantizeNormal Direction,float Impulse);
	UFUNCTION(NetMulticast,Reliable) void MulticastDeathImpact(FName Bone,FVector_NetQuantize BonePoint,FVector_NetQuantizeNormal Direction,float Impulse);
};
