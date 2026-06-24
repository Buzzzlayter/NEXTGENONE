// Copyright. NgxVoxel — detached voxel debris (VS0 step 6b).

#include "VoxelDebris.h"
#include "ProceduralMeshComponent.h"

AVoxelDebris::AVoxelDebris()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DebrisMesh"));
	SetRootComponent(Mesh);
	Mesh->bUseAsyncCooking = false;   // мелкий convex — куем синхронно, физика стартует сразу
	Mesh->SetCanEverAffectNavigation(false);
	// Простая коллизия = выпуклая оболочка (для симуляции), а не complex-из-меша.
	Mesh->bUseComplexAsSimpleCollision = false;
}

void AVoxelDebris::Init(const FVoxelMeshData& MeshData, float LifeSeconds)
{
	if (MeshData.IsEmpty())
	{
		Destroy();
		return;
	}

	// Меш без complex-коллизии; простую коллизию делаем выпуклой оболочкой по вершинам (дёшево, симулируется).
	Mesh->CreateMeshSection(
		0,
		MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UV0,
		MeshData.VertexColors,
		TArray<FProcMeshTangent>(),
		/*bCreateCollision=*/ false);

	Mesh->AddCollisionConvexMesh(MeshData.Vertices);

	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore); // не перехватывать карв-трейс
	Mesh->SetSimulatePhysics(true);

	SetLifeSpan(LifeSeconds);   // авто-деспавн осевшего куска
}
