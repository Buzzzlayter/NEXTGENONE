#pragma once

#include "Components/SceneComponent.h"
#include "UIMarkerComponent.generated.h"

class UUIMarkerModel;
class UUIMarkerSubsystem;

UCLASS(ClassGroup = UI, PrioritizeCategories = ("Marker"), meta = (BlueprintSpawnableComponent))
class UIMODULE_API UUIMarkerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UUIMarkerModel* CreateRuntimeMarker(UUIMarkerSubsystem* Subsystem, TSubclassOf<UUIMarkerModel> ModelClass) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateFlags, ETeleportType Teleport);

	UPROPERTY(EditAnywhere, Instanced, Category = "Marker")
	TObjectPtr<UUIMarkerModel> MarkerModel;

	TWeakObjectPtr<UUIMarkerModel> RegisteredModel;
};
