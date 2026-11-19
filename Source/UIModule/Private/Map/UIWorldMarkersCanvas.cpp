#include "Map/UIWorldMarkersCanvas.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Map/UIMarkerModel.h"
#include "Map/UIMarkerObserver.h"
#include "Map/UIMarkerSubsystem.h"
#include "Map/UIMarkerWidget.h"

void UUIWorldMarkersCanvas::NativeConstruct()
{
	Super::NativeConstruct();
	const auto* Subsystem = UWorld::GetSubsystem<UUIMarkerSubsystem>(GetWorld());
	if (!RootCanvas || !Subsystem) return;
	auto* Observer = Subsystem->GetMarkerObserver(GetOwningLocalPlayer());
	if (!Observer) return;

	RootCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	MarkerObserver = Observer;
	Observer->OnMarkerVisible.AddUObject(this, &ThisClass::TrackMarker);
	Observer->OnMarkerHidden.AddUObject(this, &ThisClass::UntrackMarker);
	for (auto* Model : Observer->GetVisibleMarkers(EUIMarkerSurface::World)) TrackMarker(Model, EUIMarkerSurface::World, false);
}

void UUIWorldMarkersCanvas::NativeDestruct()
{
	if (auto* Observer = MarkerObserver.Get())
	{
		Observer->OnMarkerVisible.RemoveAll(this);
		Observer->OnMarkerHidden.RemoveAll(this);
	}
	MarkerObserver.Reset();

	auto Markers = MoveTemp(TrackedMarkers);
	for (auto& Pair : Markers) DemountWidget(Pair.Value);
	Super::NativeDestruct();
}

void UUIWorldMarkersCanvas::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	auto* Controller = GetOwningPlayer();
	if (!RootCanvas || !Controller || TrackedMarkers.IsEmpty()) return;

	const auto& CanvasGeometry = RootCanvas->GetCachedGeometry();
	const FVector2D Size = CanvasGeometry.GetLocalSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0) return;
	const FBox2D Bounds(FVector2D::ZeroVector, Size);
	TArray<FGuid, TInlineAllocator<32>> MarkerIds;
	TrackedMarkers.GenerateKeyArray(MarkerIds);
	for (const auto& Id : MarkerIds)
	{
		auto* Marker = TrackedMarkers.Find(Id);
		auto* Model = Marker ? Marker->Model.Get() : nullptr;
		if (!Model) continue;

		const auto* MarkerVisibility = Model->GetVisibility(EUIMarkerSurface::World);
		const bool bKeepAtEdge = MarkerVisibility && MarkerVisibility->bKeepAtEdge;
		const FVector Position = Model->GetPosition();
		FVector2D ScreenPosition = FVector2D::ZeroVector;
		FVector2D CanvasPosition = FVector2D::ZeroVector;
		const bool bProjectedInFront = Controller->ProjectWorldLocationToScreen(Position, ScreenPosition);
		bool bProjected = bProjectedInFront;
		if (!bProjected && bKeepAtEdge)
		{
			FVector ViewLocation = FVector::ZeroVector;
			FRotator ViewRotation = FRotator::ZeroRotator;
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
			const FVector Forward = ViewRotation.Vector();
			const double Depth = FVector::DotProduct(Position - ViewLocation, Forward);
			if (Depth <= 0.0)
			{
				const FVector FrontPosition = Position + Forward * (FMath::Max(-Depth, 1.0) - Depth);
				bProjected = Controller->ProjectWorldLocationToScreen(FrontPosition, ScreenPosition);
			}
		}

		if (bProjected)
			USlateBlueprintLibrary::ScreenToWidgetLocal(this, CanvasGeometry, ScreenPosition, CanvasPosition);
		const bool bOnScreen = bProjectedInFront && Bounds.IsInsideOrOn(CanvasPosition);
		if (!bProjected || CanvasPosition.ContainsNaN() || (!bOnScreen && !bKeepAtEdge))
		{
			DemountWidget(*Marker);
			continue;
		}

		FVector2D Alignment(0.5, 0.5);
		if (!bOnScreen)
		{
			Alignment = UUIMarkerWidget::GetEdgeAnchor(CanvasPosition / Size);
			CanvasPosition = Alignment * Size;
		}

		auto* CanvasSlot = Marker->CanvasSlot.Get();
		if (!CanvasSlot)
		{
			CanvasSlot = UUIMarkerWidget::Mount(RootCanvas.Get(), Controller, Model);
			if (!CanvasSlot) continue;
			if (auto* Current = TrackedMarkers.Find(Id)) Current->CanvasSlot = CanvasSlot;
			else
			{
				RootCanvas->RemoveChild(CanvasSlot->GetContent());
				continue;
			}
		}

		CanvasSlot->SetPosition(CanvasPosition);
		if (CanvasSlot->GetAlignment() != Alignment) CanvasSlot->SetAlignment(Alignment);
	}
}

void UUIWorldMarkersCanvas::TrackMarker(UUIMarkerModel* Model, EUIMarkerSurface Surface, bool bPresentationChanged)
{
	if (Surface != EUIMarkerSurface::World || !IsValid(Model)) return;
	auto& Marker = TrackedMarkers.FindOrAdd(Model->GetMarkerId());
	Marker.Model = Model;
	if (!bPresentationChanged) return;
	if (auto* CanvasSlot = Marker.CanvasSlot.Get())
	{
		auto* Widget = Cast<UUIMarkerWidget>(CanvasSlot->GetContent());
		if (Widget && Widget->GetClass() == Model->GetWidgetClass() && Widget->GetOwningPlayer() == GetOwningPlayer())
			Widget->SetMarkerModel(Model);
		else
			DemountWidget(Marker);
	}
}

void UUIWorldMarkersCanvas::UntrackMarker(FGuid Id, EUIMarkerSurface Surface)
{
	if (Surface != EUIMarkerSurface::World) return;
	FTrackedMarker Marker;
	if (TrackedMarkers.RemoveAndCopyValue(Id, Marker)) DemountWidget(Marker);
}

void UUIWorldMarkersCanvas::DemountWidget(FTrackedMarker& Marker)
{
	auto* CanvasSlot = Marker.CanvasSlot.Get();
	Marker.CanvasSlot.Reset();
	if (CanvasSlot)
		if (auto* Widget = CanvasSlot->GetContent()) Widget->RemoveFromParent();
}
