#include "Map/UIMarkerComponent.h"

#include "Engine/World.h"
#include "Map/UIMarkerModel.h"
#include "Map/UIMarkerSubsystem.h"

void UUIMarkerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Subsystem = UWorld::GetSubsystem<UUIMarkerSubsystem>(GetWorld()))
	{
		auto* Model = UUIMarkerModel::CreateRuntimeModel(Subsystem, MarkerModel.Get(), GetComponentLocation());
		if (Subsystem->RegisterMarker(Model))
		{
			RegisteredModel = Model;
			TransformUpdated.AddUObject(this, &ThisClass::HandleTransformUpdated);
		}
	}
}

void UUIMarkerComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	TransformUpdated.RemoveAll(this);
	if (auto* Model = RegisteredModel.Get())
	{
		RegisteredModel.Reset();
		if (auto* Subsystem = UWorld::GetSubsystem<UUIMarkerSubsystem>(GetWorld())) Subsystem->UnregisterMarker(Model);
	}
	Super::EndPlay(EndPlayReason);
}

void UUIMarkerComponent::HandleTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateFlags, ETeleportType Teleport)
{
	if (auto* Model = RegisteredModel.Get()) Model->SetPosition(GetComponentLocation());
}
