// Copyright. NgxVoxel — naive face-culled mesher.

#include "VoxelMesher.h"
#include "VoxelChunk.h"
#include "VoxelTypes.h"

void FVoxelMeshData::Reset()
{
	Vertices.Reset();
	Triangles.Reset();
	Normals.Reset();
	UV0.Reset();
	VertexColors.Reset();
}

namespace
{
	// Порядок: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z.
	const FIntVector Neighbors[6] = {
		{ 1, 0, 0 }, { -1, 0, 0 },
		{ 0, 1, 0 }, { 0, -1, 0 },
		{ 0, 0, 1 }, { 0, 0, -1 }
	};

	const FVector FaceNormals[6] = {
		{ 1, 0, 0 }, { -1, 0, 0 },
		{ 0, 1, 0 }, { 0, -1, 0 },
		{ 0, 0, 1 }, { 0, 0, -1 }
	};

	// 4 угла на грань (в воксельных единицах), CCW при взгляде снаружи.
	// Если меш отрисуется наизнанку — поменять порядок треугольников (winding) в цикле ниже.
	const FVector FaceCorners[6][4] = {
		// +X
		{ { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 } },
		// -X
		{ { 0, 1, 0 }, { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 } },
		// +Y
		{ { 1, 1, 0 }, { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 } },
		// -Y
		{ { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } },
		// +Z
		{ { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } },
		// -Z
		{ { 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 }, { 0, 0, 0 } }
	};

	FColor MaterialColor(uint8 Material)
	{
		switch (Material)
		{
		case 1:  return FColor(170, 160, 145); // камень
		case 2:  return FColor(120, 110, 90);  // укреплённый
		default: return FColor(200, 70, 60);   // слабый/прочее
		}
	}
}

void FVoxelMesher::GenerateNaive(const FVoxelChunk& Chunk, FVoxelMeshData& Out)
{
	Out.Reset();

	const float S = NgxVoxel::VoxelSizeCm;
	const int32 N = NgxVoxel::ChunkSize;

	for (int32 Z = 0; Z < N; ++Z)
	for (int32 Y = 0; Y < N; ++Y)
	for (int32 X = 0; X < N; ++X)
	{
		const uint8 Material = Chunk.Get(X, Y, Z);
		if (Material == 0)
		{
			continue;
		}

		const FVector Base((float)X * S, (float)Y * S, (float)Z * S);

		for (int32 F = 0; F < 6; ++F)
		{
			const FIntVector Nbr = Neighbors[F];
			if (Chunk.IsSolid(X + Nbr.X, Y + Nbr.Y, Z + Nbr.Z))
			{
				continue; // грань скрыта соседом
			}

			const int32 V0 = Out.Vertices.Num();
			const FColor Col = MaterialColor(Material);

			for (int32 C = 0; C < 4; ++C)
			{
				Out.Vertices.Add(Base + FaceCorners[F][C] * S);
				Out.Normals.Add(FaceNormals[F]);
				Out.UV0.Add(FVector2D((C == 1 || C == 2) ? 1.f : 0.f, (C >= 2) ? 1.f : 0.f));
				Out.VertexColors.Add(Col);
			}

			// Winding: CW при взгляде снаружи (UE front-face). Если меш наизнанку — переставить.
			Out.Triangles.Add(V0 + 0);
			Out.Triangles.Add(V0 + 2);
			Out.Triangles.Add(V0 + 1);
			Out.Triangles.Add(V0 + 0);
			Out.Triangles.Add(V0 + 3);
			Out.Triangles.Add(V0 + 2);
		}
	}
}
