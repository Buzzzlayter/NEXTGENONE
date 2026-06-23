// Copyright. NgxVoxel — chunk meshing.

#pragma once

#include "CoreMinimal.h"

struct FVoxelChunk;

// Буферы под ProceduralMeshComponent::CreateMeshSection.
struct NGXVOXEL_API FVoxelMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FColor> VertexColors;

	void Reset();
	bool IsEmpty() const { return Triangles.Num() == 0; }
};

class NGXVOXEL_API FVoxelMesher
{
public:
	// Наивный face-culled меш одного чанка. Greedy — следующий шаг (та же сигнатура).
	static void GenerateNaive(const FVoxelChunk& Chunk, FVoxelMeshData& Out);
};
