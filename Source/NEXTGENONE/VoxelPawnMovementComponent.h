// Copyright. NEXTGENONE - voxel-grounded pawn movement component.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoxelPawnMovementComponent.generated.h"

class USceneComponent;

UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class NEXTGENONE_API UVoxelPawnMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVoxelPawnMovementComponent();

	void SetUpdatedComponent(USceneComponent* InUpdatedComponent);
	void SetFocusComponent(USceneComponent* InFocusComponent);

	bool Jump();
	bool IsGroundedOnVoxel() const { return bIsGroundedOnVoxel; }

	int32 GetCachedNumVoxelActors() const { return CachedNumVoxelActors; }
	int32 GetCachedNumChunks() const { return CachedNumChunks; }
	int32 GetCachedNumSections() const { return CachedNumSections; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel")
	bool bWalkOnVoxels = true;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel", meta = (ClampMin = "0"))
	float EyeHeightCm = 170.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel", meta = (ClampMin = "0"))
	float GroundProbeDistanceCm = 220.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel", meta = (ClampMin = "0"))
	float GroundSnapDistanceCm = 45.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel", meta = (ClampMin = "0"))
	float MaxStepHeightCm = 65.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel", meta = (ClampMin = "0"))
	float GravityCmS2 = 2600.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel", meta = (ClampMin = "0"))
	float JumpSpeedCmS = 850.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel", meta = (ClampMin = "0"))
	float MaxFallSpeedCmS = 4200.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel", meta = (ClampMin = "0"))
	float GroundFollowInterpSpeed = 18.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Voxel", meta = (ClampMin = "0.01"))
	float VoxelFocusUpdateInterval = 0.15f;

private:
	void UpdateVoxelFocusAndMetrics(float DeltaSeconds);
	void UpdateVoxelWalking(float DeltaSeconds);
	bool FindVoxelGround(FVector& OutImpact) const;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> UpdatedComponent;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> FocusComponent;

	float VerticalVelocityCmS = 0.f;
	bool bIsGroundedOnVoxel = false;
	float VoxelFocusUpdateTimer = 0.f;
	int32 CachedNumVoxelActors = 0;
	int32 CachedNumChunks = 0;
	int32 CachedNumSections = 0;
};
