// Copyright. NgxVoxel — core voxel constants and helpers.

#pragma once

#include "CoreMinimal.h"

// 0 = пусто; иначе material id.
using FVoxel = uint8;

namespace NgxVoxel
{
	// Размер вокселя в UE-юнитах (см). Стартовая точка — валидировать на перфе.
	static constexpr float VoxelSizeCm = 5.0f;

	// Сторона чанка в вокселях.
	static constexpr int32 ChunkSize = 16;
	static constexpr int32 ChunkSizeCubed = ChunkSize * ChunkSize * ChunkSize;

	FORCEINLINE int32 VoxelIndex(int32 X, int32 Y, int32 Z)
	{
		return X + Y * ChunkSize + Z * ChunkSize * ChunkSize;
	}

	FORCEINLINE bool InBounds(int32 X, int32 Y, int32 Z)
	{
		return X >= 0 && X < ChunkSize
			&& Y >= 0 && Y < ChunkSize
			&& Z >= 0 && Z < ChunkSize;
	}
}
