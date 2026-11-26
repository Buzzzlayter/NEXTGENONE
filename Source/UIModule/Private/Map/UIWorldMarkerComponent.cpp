#include "Map/UIWorldMarkerComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Map/UIMarkerModel.h"
#include "Map/UIMarkerSubsystem.h"

UUIWorldMarkerComponent::UUIWorldMarkerComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetWidgetSpace(EWidgetSpace::World);
	SetOnlyOwnerSee(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetWindowFocusable(false);
	SetDrawAtDesiredSize(true);
	SetRelativeScale3D(FVector(0.15));
}

void UUIWorldMarkerComponent::InitWidget()
{
}

bool UUIWorldMarkerComponent::ShowMarker(APlayerController* Viewer, UUIMarkerModel* Model, TSubclassOf<UUIMarkerWidget> PresentationClass)
{
	auto* Subsystem = UWorld::GetSubsystem<UUIMarkerSubsystem>(GetWorld());
	if (ActiveMarker || !Viewer || !Viewer->GetLocalPlayer() || !Subsystem || !IsValid(Model)) return false;

	auto* MarkerWidget = CreateWidget<UUIMarkerWidget>(Viewer, PresentationClass);
	if (!MarkerWidget || !Subsystem->RegisterMarker(Model)) return false;

	ActiveMarker = Model;
	SetOwnerPlayer(Viewer->GetLocalPlayer());
	MarkerWidget->SetMarkerModel(Model);
	SetWidget(MarkerWidget);
	SetComponentTickEnabled(true);
	UpdateWidgetTransform(*Viewer);
	Model->OnMarkerChanged().AddUObject(this, &ThisClass::MarkerChanged);
	return true;
}

void UUIWorldMarkerComponent::EndPlay(EEndPlayReason::Type Reason)
{
	if (ActiveMarker)
	{
		ActiveMarker->OnMarkerChanged().RemoveAll(this);
		if (auto* Subsystem = UWorld::GetSubsystem<UUIMarkerSubsystem>(GetWorld()))
			Subsystem->UnregisterMarker(ActiveMarker.Get());
		ActiveMarker = nullptr;
	}
	Super::EndPlay(Reason);
}

void UUIWorldMarkerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	if (const auto* MarkerWidget = GetUserWidgetObject(); ActiveMarker && MarkerWidget)
		if (const auto* Viewer = MarkerWidget->GetOwningPlayer()) UpdateWidgetTransform(*Viewer);
	Super::TickComponent(DeltaTime, TickType, TickFunction);
}

void UUIWorldMarkerComponent::MarkerChanged(UUIMarkerModel* Model, bool bPresentationChanged)
{
	if (bPresentationChanged)
		if (auto* MarkerWidget = Cast<UUIMarkerWidget>(GetUserWidgetObject())) MarkerWidget->SetMarkerModel(Model);
}

void UUIWorldMarkerComponent::UpdateWidgetTransform(const APlayerController& Viewer)
{
	FVector CameraPosition;
	FRotator CameraRotation;
	Viewer.GetPlayerViewPoint(CameraPosition, CameraRotation);
	const FVector MarkerPosition = ActiveMarker->GetPosition();
	SetWorldLocationAndRotation(MarkerPosition, (CameraPosition - MarkerPosition).Rotation());
}
