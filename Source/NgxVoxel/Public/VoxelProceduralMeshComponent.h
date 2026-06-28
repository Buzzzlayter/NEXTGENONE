// Copyright. NgxVoxel - procedural mesh component with one shared material slot.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "VoxelProceduralMeshComponent.generated.h"

class UMaterialInterface;

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class NGXVOXEL_API UVoxelProceduralMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TObjectPtr<UMaterialInterface> SharedMaterial;

	virtual int32 GetNumMaterials() const override
	{
		return SharedMaterial ? 1 : Super::GetNumMaterials();
	}

	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override
	{
		return SharedMaterial ? SharedMaterial.Get() : Super::GetMaterial(ElementIndex);
	}
};
