// Copyright. NEXTGENONE - voxel-grounded pawn movement component.

#include "VoxelPawnMovementComponent.h"

#include "VoxelRuntimeRegistry.h"
#include "VoxelStructure.h"
#include "VoxelWorld.h"
#include "Components/SceneComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/Pawn.h"

UVoxelPawnMovementComponent::UVoxelPawnMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVoxelPawnMovementComponent::SetUpdatedComponent(USceneComponent* InUpdatedComponent)
{
	UpdatedComponent = InUpdatedComponent;
}

void UVoxelPawnMovementComponent::SetFocusComponent(USceneComponent* InFocusComponent)
{
	FocusComponent = InFocusComponent;
}

void UVoxelPawnMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!UpdatedComponent)
	{
		UpdatedComponent = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
	}
	if (!FocusComponent)
	{
		FocusComponent = UpdatedComponent;
	}

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (UFloatingPawnMovement* FloatingMovement = Pawn->FindComponentByClass<UFloatingPawnMovement>())
		{
			FloatingMovement->SetPlaneConstraintNormal(FVector::UpVector);
			FloatingMovement->SetPlaneConstraintEnabled(true);
			PrimaryComponentTick.AddPrerequisite(FloatingMovement, FloatingMovement->PrimaryComponentTick);
		}
	}
}

void UVoxelPawnMovementComponent::TickComponent(float DeltaSeconds, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	UpdateVoxelFocusAndMetrics(DeltaSeconds);
	if (bWalkOnVoxels)
	{
		UpdateVoxelWalking(DeltaSeconds);
	}
}

bool UVoxelPawnMovementComponent::Jump()
{
	if (!bWalkOnVoxels || !bIsGroundedOnVoxel)
	{
		return false;
	}

	VerticalVelocityCmS = JumpSpeedCmS;
	bIsGroundedOnVoxel = false;
	return true;
}

void UVoxelPawnMovementComponent::UpdateVoxelFocusAndMetrics(float DeltaSeconds)
{
	VoxelFocusUpdateTimer -= DeltaSeconds;
	if (VoxelFocusUpdateTimer > 0.f)
	{
		return;
	}
	VoxelFocusUpdateTimer = VoxelFocusUpdateInterval;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CachedNumVoxelActors = 0;
	CachedNumChunks = 0;
	CachedNumSections = 0;

	const FVector Focus = FocusComponent ? FocusComponent->GetComponentLocation()
		: (UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector);

	FVoxelRuntimeRegistry::ForEachStructure(World, [this, Focus](AVoxelStructure& Structure)
	{
		Structure.SetCollisionFocus(Focus);
		++CachedNumVoxelActors;
		CachedNumChunks += Structure.GetNumChunks();
		CachedNumSections += Structure.GetNumSections();
	});

	FVoxelRuntimeRegistry::ForEachWorld(World, [this](AVoxelWorld& VoxelWorld)
	{
		++CachedNumVoxelActors;
		CachedNumChunks += VoxelWorld.GetLoadedChunkCount();
		CachedNumSections += VoxelWorld.GetLoadedChunkCount();
	});
}

bool UVoxelPawnMovementComponent::FindVoxelGround(FVector& OutImpact) const
{
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return false;
	}

	const FVector Location = Owner->GetActorLocation();
	const FVector Start = Location + FVector(0.f, 0.f, 20.f);
	const FVector End = Location - FVector(0.f, 0.f, EyeHeightCm + GroundProbeDistanceCm);
	float BestDistSq = TNumericLimits<float>::Max();
	bool bHit = false;

	FVoxelRuntimeRegistry::ForEachStructure(World, [&](AVoxelStructure& Structure)
	{
		FVector Impact;
		FVector Normal;
		if (Structure.RaycastSolidVoxel(Start, End, Impact, Normal))
		{
			const float DistSq = FVector::DistSquared(Start, Impact);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutImpact = Impact;
				bHit = true;
			}
		}
	});

	FVoxelRuntimeRegistry::ForEachWorld(World, [&](AVoxelWorld& VoxelWorld)
	{
		FVector Impact;
		FVector Normal;
		if (VoxelWorld.RaycastSolidVoxel(Start, End, Impact, Normal))
		{
			const float DistSq = FVector::DistSquared(Start, Impact);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutImpact = Impact;
				bHit = true;
			}
		}
	});

	return bHit;
}

void UVoxelPawnMovementComponent::UpdateVoxelWalking(float DeltaSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner || DeltaSeconds <= 0.f)
	{
		return;
	}

	FVector GroundImpact = FVector::ZeroVector;
	const bool bHasGround = FindVoxelGround(GroundImpact);

	FVector Location = Owner->GetActorLocation();
	if (bHasGround)
	{
		const float DesiredZ = GroundImpact.Z + EyeHeightCm;
		const float DeltaToGround = DesiredZ - Location.Z;
		const bool bCanSnapDown = DeltaToGround >= -GroundSnapDistanceCm;
		const bool bCanStepUp = DeltaToGround <= MaxStepHeightCm;

		if (VerticalVelocityCmS <= 0.f && bCanSnapDown && bCanStepUp)
		{
			if (DeltaToGround > 0.f)
			{
				Location.Z = DesiredZ;
			}
			else
			{
				Location.Z = FMath::FInterpTo(Location.Z, DesiredZ, DeltaSeconds, GroundFollowInterpSpeed);
			}
			Owner->SetActorLocation(Location, false);
			VerticalVelocityCmS = 0.f;
			bIsGroundedOnVoxel = true;
			return;
		}
	}

	bIsGroundedOnVoxel = false;
	VerticalVelocityCmS = FMath::Max(VerticalVelocityCmS - GravityCmS2 * DeltaSeconds, -MaxFallSpeedCmS);
	Location.Z += VerticalVelocityCmS * DeltaSeconds;

	if (bHasGround)
	{
		const float DesiredZ = GroundImpact.Z + EyeHeightCm;
		if (VerticalVelocityCmS <= 0.f && Location.Z <= DesiredZ)
		{
			Location.Z = DesiredZ;
			VerticalVelocityCmS = 0.f;
			bIsGroundedOnVoxel = true;
		}
	}

	Owner->SetActorLocation(Location, false);
}
