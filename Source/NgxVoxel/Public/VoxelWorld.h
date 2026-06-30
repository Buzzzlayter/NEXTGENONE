// Copyright. NgxVoxel - streaming voxel world actor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelChunk.h"
#include "VoxelMesher.h"
#include "Containers/Queue.h"
#include "Templates/SharedPointer.h"
#include "VoxelWorld.generated.h"

class UMaterialInterface;
class USceneComponent;
class UVoxelProceduralMeshComponent;

struct FVoxelWorldMeshResult
{
	FIntVector Coord = FIntVector(0, 0, 0);
	uint64 Version = 0;
	FVoxelMeshData Data;
};

struct FVoxelWorldMeshPipeline
{
	TQueue<FVoxelWorldMeshResult, EQueueMode::Mpsc> Completed;
};

struct FVoxelWorldChunk
{
	FVoxelChunk Data;
	UVoxelProceduralMeshComponent* Mesh = nullptr;
	uint64 Version = 0;
};

UCLASS(meta = (PrioritizeCategories = "Voxel"))
class NGXVOXEL_API AVoxelWorld : public AActor
{
	GENERATED_BODY()

public:
	AVoxelWorld();

	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, Category = "Voxel|World")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, Category = "Voxel|World", meta = (ClampMin = "0"))
	int32 ViewDistanceChunks = 12;

	UPROPERTY(EditAnywhere, Category = "Voxel|World", meta = (ClampMin = "0"))
	float ViewDistanceCm = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Voxel|World", meta = (ClampMin = "1"))
	int32 ChunkHeightChunks = 4;

	UPROPERTY(EditAnywhere, Category = "Voxel|World")
	TObjectPtr<AActor> FocusActor;

	UPROPERTY(EditAnywhere, Category = "Voxel|World", meta = (ClampMin = "0.05"))
	float StreamingUpdateInterval = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Voxel|Meshing")
	bool bUseGreedy = true;

	UPROPERTY(EditAnywhere, Category = "Voxel|Meshing", meta = (ClampMin = "1"))
	int32 MaxChunkLoadsPerUpdate = 32;

	UPROPERTY(EditAnywhere, Category = "Voxel|Meshing", meta = (ClampMin = "1"))
	int32 DispatchBudgetPerFrame = 8;

	UPROPERTY(EditAnywhere, Category = "Voxel|Meshing", meta = (ClampMin = "1"))
	int32 ApplyBudgetPerFrame = 8;

	UPROPERTY(EditAnywhere, Category = "Voxel|Collision")
	bool bCreateCollision = false;

	UPROPERTY(EditAnywhere, Category = "Voxel|Material")
	TObjectPtr<UMaterialInterface> VoxelMaterial;

	UFUNCTION(CallInEditor, Category = "Voxel")
	void RebuildWorld();

	int32 GetLoadedChunkCount() const { return Chunks.Num(); }
	int32 ApplyDamageSphere(const FVector& WorldCenter, float RadiusCm, uint8 NewMaterial);

	bool RaycastSolidVoxel(const FVector& WorldStart, const FVector& WorldEnd,
		FVector& OutWorldImpact, FVector& OutWorldNormal) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TMap<FIntVector, FVoxelWorldChunk> Chunks;
	TSet<FIntVector> DirtyChunks;
	TSet<FIntVector> InFlight;
	TSharedPtr<FVoxelWorldMeshPipeline, ESPMode::ThreadSafe> Pipeline;

	float TimeUntilStreamingUpdate = 0.f;

	void UpdateStreamingWindow();
	void GenerateChunk(const FIntVector& Coord);
	void UnloadChunk(const FIntVector& Coord);

	void MarkChunkAndNeighborsDirty(const FIntVector& Coord);
	void DispatchChunk(const FIntVector& Coord);
	void ApplyResult(FVoxelWorldMeshResult& Result);

	FVector ChunkOrigin(const FIntVector& Coord) const;
	FIntVector WorldToChunkCoord(const FVector& WorldLocation) const;

	uint8 GenerateMaterialAt(int32 GlobalX, int32 GlobalY, int32 GlobalZ) const;
	uint8 GetMaterialAtGlobalVoxel(const FIntVector& GlobalVoxel) const;
	FVoxelWorldChunk* FindChunkForGlobalVoxel(const FIntVector& GlobalVoxel, FIntVector& OutChunkCoord,
		FIntVector& OutLocalVoxel);
	const FVoxelWorldChunk* FindChunkForGlobalVoxel(const FIntVector& GlobalVoxel, FIntVector& OutChunkCoord,
		FIntVector& OutLocalVoxel) const;
	static int32 FloorDiv(int32 Value, int32 Divisor);
	static int32 PositiveMod(int32 Value, int32 Divisor);
	AActor* ResolveFocusActor() const;
};
