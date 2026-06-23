// Copyright. NgxVoxel — destructible voxel structure actor (VS0 stub).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelChunk.h"
#include "VoxelStructure.generated.h"

class UProceduralMeshComponent;

UCLASS()
class NGXVOXEL_API AVoxelStructure : public AActor
{
	GENERATED_BODY()

public:
	AVoxelStructure();

	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	// Материал тестового заполнения (1=камень, 2=укреплённый).
	UPROPERTY(EditAnywhere, Category = "Voxel")
	uint8 TestMaterial = 1;

	// Пересобрать тестовый чанк и меш (видно прямо в редакторе).
	UFUNCTION(CallInEditor, Category = "Voxel")
	void Rebuild();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	FVoxelChunk Chunk;

	void FillTestPattern();
	void BuildMesh();
};
