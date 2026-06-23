// Copyright. NgxVoxel — flat voxel chunk (plain data, not a UObject).

#pragma once

#include "CoreMinimal.h"
#include "VoxelTypes.h"

struct NGXVOXEL_API FVoxelChunk
{
	// Плоский массив материалов, размер ChunkSizeCubed. 0 = пусто.
	TArray<uint8> Voxels;

	FVoxelChunk();

	void Fill(uint8 Material);
	void Clear();

	FORCEINLINE uint8 Get(int32 X, int32 Y, int32 Z) const
	{
		if (!NgxVoxel::InBounds(X, Y, Z))
		{
			return 0;
		}
		return Voxels[NgxVoxel::VoxelIndex(X, Y, Z)];
	}

	FORCEINLINE void Set(int32 X, int32 Y, int32 Z, uint8 Material)
	{
		if (NgxVoxel::InBounds(X, Y, Z))
		{
			Voxels[NgxVoxel::VoxelIndex(X, Y, Z)] = Material;
		}
	}

	FORCEINLINE bool IsSolid(int32 X, int32 Y, int32 Z) const
	{
		return Get(X, Y, Z) != 0;
	}
};
