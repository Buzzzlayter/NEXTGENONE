#include "Map/UIMarkerComponent.h"

#include "Engine/World.h"
#include "Map/UIMarkerModel.h"
#include "Map/UIMarkerSubsystem.h"

UUIMarkerModel* UUIMarkerComponent::CreateRuntimeMarker(UUIMarkerSubsystem* Subsystem, TSubclassOf<UUIMarkerModel> ModelClass) const
{
	if (!IsValid(MarkerModel) || (ModelClass && !MarkerModel->IsA(ModelClass.Get()))) return nullptr;
	return UUIMarkerModel::CreateRuntimeModel(Subsystem, MarkerModel.Get(), GetComponentLocation());
}

void UUIMarkerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!IsValid(MarkerModel) || !MarkerModel->ShouldRegisterOnBeginPlay()) return;
	if (auto* Subsystem = UWorld::GetSubsystem<UUIMarkerSubsystem>(GetWorld()))
	{
		auto* Model = CreateRuntimeMarker(Subsystem, nullptr);
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
