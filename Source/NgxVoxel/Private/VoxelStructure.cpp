// Copyright. NgxVoxel — AVoxelStructure implementation (VS0).

#include "VoxelStructure.h"
#include "VoxelMesher.h"
#include "VoxelTypes.h"
#include "ProceduralMeshComponent.h"

AVoxelStructure::AVoxelStructure()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("VoxelMesh"));
	SetRootComponent(Mesh);
	Mesh->bUseAsyncCooking = true;
}

void AVoxelStructure::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AVoxelStructure::Rebuild()
{
	FillTestPattern();
	BuildMesh();
}

void AVoxelStructure::FillTestPattern()
{
	Chunk.Clear();

	const int32 N = NgxVoxel::ChunkSize;
	const FVector Center((N - 1) * 0.5f, (N - 1) * 0.5f, (N - 1) * 0.5f);
	const float Radius = N * 0.45f;

	// Сплошной воксельный шар — наглядно видно ступенчатую поверхность и winding.
	for (int32 Z = 0; Z < N; ++Z)
	for (int32 Y = 0; Y < N; ++Y)
	for (int32 X = 0; X < N; ++X)
	{
		const float Dist = (FVector((float)X, (float)Y, (float)Z) - Center).Size();
		Chunk.Set(X, Y, Z, (Dist <= Radius) ? TestMaterial : 0);
	}
}

void AVoxelStructure::BuildMesh()
{
	FVoxelMeshData Data;
	FVoxelMesher::GenerateNaive(Chunk, Data);

	Mesh->ClearAllMeshSections();
	if (!Data.IsEmpty())
	{
		Mesh->CreateMeshSection(
			0,
			Data.Vertices,
			Data.Triangles,
			Data.Normals,
			Data.UV0,
			Data.VertexColors,
			TArray<FProcMeshTangent>(),
			/*bCreateCollision=*/ true);
	}
}
