// Copyright. NgxVoxel - lightweight runtime registry for voxel actors.

#include "VoxelRuntimeRegistry.h"

#include "VoxelStructure.h"
#include "VoxelWorld.h"
#include "Engine/World.h"

namespace
{
	template <typename TActor>
	void RegisterActor(TArray<TWeakObjectPtr<TActor>>& Actors, TActor* Actor)
	{
		if (!Actor)
		{
			return;
		}

		for (int32 Index = Actors.Num() - 1; Index >= 0; --Index)
		{
			TActor* Existing = Actors[Index].Get();
			if (!Existing)
			{
				Actors.RemoveAtSwap(Index);
				continue;
			}
			if (Existing == Actor)
			{
				return;
			}
		}

		Actors.Add(Actor);
	}

	template <typename TActor>
	void UnregisterActor(TArray<TWeakObjectPtr<TActor>>& Actors, TActor* Actor)
	{
		for (int32 Index = Actors.Num() - 1; Index >= 0; --Index)
		{
			TActor* Existing = Actors[Index].Get();
			if (!Existing || Existing == Actor)
			{
				Actors.RemoveAtSwap(Index);
			}
		}
	}

	template <typename TActor>
	void ForEachActor(TArray<TWeakObjectPtr<TActor>>& Actors, const UWorld* World, TFunctionRef<void(TActor&)> Func)
	{
		for (int32 Index = Actors.Num() - 1; Index >= 0; --Index)
		{
			TActor* Actor = Actors[Index].Get();
			if (!Actor)
			{
				Actors.RemoveAtSwap(Index);
				continue;
			}
			if (!World || Actor->GetWorld() == World)
			{
				Func(*Actor);
			}
		}
	}
}

void FVoxelRuntimeRegistry::Register(AVoxelStructure* Structure)
{
	RegisterActor(Structures(), Structure);
}

void FVoxelRuntimeRegistry::Unregister(AVoxelStructure* Structure)
{
	UnregisterActor(Structures(), Structure);
}

void FVoxelRuntimeRegistry::Register(AVoxelWorld* World)
{
	RegisterActor(Worlds(), World);
}

void FVoxelRuntimeRegistry::Unregister(AVoxelWorld* World)
{
	UnregisterActor(Worlds(), World);
}

void FVoxelRuntimeRegistry::ForEachStructure(const UWorld* World, TFunctionRef<void(AVoxelStructure&)> Func)
{
	ForEachActor(Structures(), World, Func);
}

void FVoxelRuntimeRegistry::ForEachWorld(const UWorld* World, TFunctionRef<void(AVoxelWorld&)> Func)
{
	ForEachActor(Worlds(), World, Func);
}

TArray<TWeakObjectPtr<AVoxelStructure>>& FVoxelRuntimeRegistry::Structures()
{
	static TArray<TWeakObjectPtr<AVoxelStructure>> RegisteredStructures;
	return RegisteredStructures;
}

TArray<TWeakObjectPtr<AVoxelWorld>>& FVoxelRuntimeRegistry::Worlds()
{
	static TArray<TWeakObjectPtr<AVoxelWorld>> RegisteredWorlds;
	return RegisteredWorlds;
}
