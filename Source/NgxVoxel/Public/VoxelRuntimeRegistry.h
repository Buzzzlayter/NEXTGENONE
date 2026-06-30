// Copyright. NgxVoxel - lightweight runtime registry for voxel actors.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AVoxelStructure;
class AVoxelWorld;
class UWorld;

class NGXVOXEL_API FVoxelRuntimeRegistry
{
public:
	static void Register(AVoxelStructure* Structure);
	static void Unregister(AVoxelStructure* Structure);

	static void Register(AVoxelWorld* World);
	static void Unregister(AVoxelWorld* World);

	static void ForEachStructure(const UWorld* World, TFunctionRef<void(AVoxelStructure&)> Func);
	static void ForEachWorld(const UWorld* World, TFunctionRef<void(AVoxelWorld&)> Func);

private:
	static TArray<TWeakObjectPtr<AVoxelStructure>>& Structures();
	static TArray<TWeakObjectPtr<AVoxelWorld>>& Worlds();
};
