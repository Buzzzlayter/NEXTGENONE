// Copyright. NgxVoxel — detached voxel debris.

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
	// LinearVelocity/AngularVelocityRad — стартовый импульс куска (мир): «ось разрушения»
	// (радиально от взрыва / вниз+спин от каскада). Ноль → просто падает под гравитацией.
	void Init(const FVoxelMeshData& MeshData, float LifeSeconds,
		const FVector& LinearVelocity = FVector::ZeroVector,
		const FVector& AngularVelocityRad = FVector::ZeroVector);

	UPROPERTY(VisibleAnywhere, Category = "Debris")
	TObjectPtr<UProceduralMeshComponent> Mesh;
};
