// Copyright. NgxVoxel — FVoxelChunk implementation.

#include "VoxelChunk.h"

FVoxelChunk::FVoxelChunk()
{
	Voxels.SetNumZeroed(NgxVoxel::ChunkSizeCubed);
}

void FVoxelChunk::Fill(uint8 Material)
{
	for (uint8& V : Voxels)
	{
		V = Material;
	}
}

void FVoxelChunk::Clear()
{
	FMemory::Memzero(Voxels.GetData(), Voxels.Num());
}
