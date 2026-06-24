// Copyright. NgxVoxel — detached voxel debris (VS0 step 6b).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelMesher.h"   // FVoxelMeshData
#include "VoxelDebris.generated.h"

class UProceduralMeshComponent;

// Оторванный кусок структуры: меш + выпуклая коллизия + симуляция физики (падает).
// Чисто воксельная физика, без игровых зависимостей.
UCLASS()
class NGXVOXEL_API AVoxelDebris : public AActor
{
	GENERATED_BODY()

public:
	AVoxelDebris();

	// Залить готовый меш (вершины в локали актёра), включить физику, задать время жизни.
	void Init(const FVoxelMeshData& MeshData, float LifeSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Debris")
	TObjectPtr<UProceduralMeshComponent> Mesh;
};
