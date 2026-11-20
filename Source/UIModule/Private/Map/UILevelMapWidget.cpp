#include "Map/UILevelMapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Map/UIMapCapture.h"
#include "Map/UIMarkerModel.h"
#include "Map/UIMarkerObserver.h"
#include "Map/UIMarkerSubsystem.h"
#include "Map/UIMarkerWidget.h"

TSharedRef<SWidget> UUILevelMapWidget::RebuildWidget()
{
	if (!IsDesignTime() && WidgetTree && MapImage && !MarkerCanvas && WidgetTree->RootWidget == MapImage.Get())
	{
		auto* Layers = WidgetTree->ConstructWidget<UOverlay>();
		MarkerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();

		auto* ImageLayer = Layers->AddChildToOverlay(MapImage.Get());
		ImageLayer->SetHorizontalAlignment(HAlign_Fill);
		ImageLayer->SetVerticalAlignment(VAlign_Fill);

		auto* MarkerLayer = Layers->AddChildToOverlay(MarkerCanvas.Get());
		MarkerLayer->SetHorizontalAlignment(HAlign_Fill);
		MarkerLayer->SetVerticalAlignment(VAlign_Fill);

		WidgetTree->RootWidget = Layers;
	}
	return Super::RebuildWidget();
}

void UUILevelMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	auto* World = GetWorld();
	if (!MapImage || !World) return;

	MapImage->SetBrushResourceObject(nullptr);
	for (TActorIterator<AUIMapCapture> It(World); It; ++It)
	{
		auto* Texture = It->GetOrCaptureMap();
		if (!Texture) continue;
		MapSource = *It;
		MapImage->SetBrushResourceObject(Texture);
		MapImage->SetVisibility(ESlateVisibility::Visible);
		break;
	}
	if (!MapSource.IsValid() || !MarkerCanvas) return;
	MarkerCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	MarkerCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	const auto* Subsystem = World->GetSubsystem<UUIMarkerSubsystem>();
	auto* Observer = Subsystem ? Subsystem->GetMarkerObserver(GetOwningLocalPlayer()) : nullptr;
	if (!Observer) return;

	MarkerObserver = Observer;
	Observer->OnMarkerVisible.AddUObject(this, &ThisClass::MountOrUpdateMarker);
	Observer->OnMarkerHidden.AddUObject(this, &ThisClass::DemountMarker);
}

void UUILevelMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateMapView();
}

void UUILevelMapWidget::UpdateMapView()
{
	if (!MapSource.IsValid() || !MapImage) return;

	const FVector2D Size = MapImage->GetCachedGeometry().GetLocalSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0) return;

	const double Aspect = Size.X / Size.Y;
	const FVector2D VisibleFraction = FVector2D(FMath::Min(Aspect, 1.0), FMath::Min(1.0 / Aspect, 1.0)) / Zoom;
	const FVector2D HalfSize = VisibleFraction * 0.5;
	const FVector2D MaxCenter = FVector2D(1.0, 1.0) - HalfSize;
	MapCenterUV.X = FMath::Clamp(MapCenterUV.X, HalfSize.X, MaxCenter.X);
	MapCenterUV.Y = FMath::Clamp(MapCenterUV.Y, HalfSize.Y, MaxCenter.Y);

	const FBox2d NewRegion(MapCenterUV - HalfSize, MapCenterUV + HalfSize);
	if (MapUVRegion.bIsValid && NewRegion.Min.Equals(MapUVRegion.Min) && NewRegion.Max.Equals(MapUVRegion.Max)) return;

	MapUVRegion = NewRegion;
	FSlateBrush Brush = MapImage->GetBrush();
	Brush.SetUVRegion(MapUVRegion);
	MapImage->SetBrush(Brush);

	if (LocalPing) MountOrUpdateMarker(LocalPing.Get(), EUIMarkerSurface::Map, false);
	if (const auto* Observer = MarkerObserver.Get())
		for (auto* Model : Observer->GetVisibleMarkers(EUIMarkerSurface::Map)) MountOrUpdateMarker(Model, EUIMarkerSurface::Map, false);
}

void UUILevelMapWidget::PanMap(FVector2D ViewDelta)
{
	if (!MapUVRegion.bIsValid || ViewDelta.ContainsNaN()) return;

	MapCenterUV -= ViewDelta * MapUVRegion.GetSize();
	UpdateMapView();
}

void UUILevelMapWidget::ZoomMap(double ZoomDelta)
{
	ZoomMapAt(ZoomDelta, FVector2D(0.5, 0.5));
}

void UUILevelMapWidget::ZoomMapAt(double ZoomDelta, const FVector2D& ViewAnchor)
{
	if (!MapUVRegion.bIsValid || !FMath::IsFinite(ZoomDelta)) return;

	const double NewZoom = FMath::Clamp(Zoom * FMath::Pow(1.2, ZoomDelta), 1.0, 8.0);
	if (Zoom == NewZoom) return;

	const FVector2D OffsetFromCenter = (ViewAnchor - FVector2D(0.5, 0.5)) * MapUVRegion.GetSize();
	MapCenterUV += OffsetFromCenter * (1.0 - Zoom / NewZoom);
	Zoom = NewZoom;
	UpdateMapView();
}

FReply UUILevelMapWidget::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!MapUVRegion.bIsValid) return Super::NativeOnMouseWheel(Geometry, Event);

	const auto& ImageGeometry = MapImage->GetCachedGeometry();
	const FVector2D Size = ImageGeometry.GetLocalSize();
	const FVector2D LocalCursorPosition = ImageGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (Size.X <= 0.0 || Size.Y <= 0.0 || !FBox2d(FVector2D::ZeroVector, Size).IsInsideOrOn(LocalCursorPosition))
		return Super::NativeOnMouseWheel(Geometry, Event);

	ZoomMapAt(Event.GetWheelDelta(), LocalCursorPosition / Size);
	return FReply::Handled();
}

void UUILevelMapWidget::RequestMapPingAtCenter()
{
	RequestMapPing(FVector2D(0.5, 0.5));
}

void UUILevelMapWidget::RequestMapPing(const FVector2D& ViewPosition)
{
	const auto* Source = MapSource.Get();
	if (!Source || !MarkerCanvas || !MapUVRegion.bIsValid) return;

	const FVector2D MapUV = MapUVRegion.Min + ViewPosition * MapUVRegion.GetSize();
	const FVector WorldPosition = Source->MapUVToWorld(MapUV);
	if (LocalPing)
	{
		LocalPing->SetPosition(WorldPosition);
	}
	else
	{
		auto* TemplateModel = LocalPingTemplate.Get();
		if (!TemplateModel) TemplateModel = GetMutableDefault<UUIMarkerModel>();
		LocalPing = UUIMarkerModel::CreateRuntimeModel(this, TemplateModel, WorldPosition);
	}

	MountOrUpdateMarker(LocalPing.Get(), EUIMarkerSurface::Map, false);
	OnMapPingRequested.Broadcast(WorldPosition);
}

FReply UUILevelMapWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (MapUVRegion.bIsValid)
	{
		const auto& ImageGeometry = MapImage->GetCachedGeometry();
		const FVector2D Size = ImageGeometry.GetLocalSize();
		const FVector2D LocalCursorPosition = ImageGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		if (Size.X > 0.0 && Size.Y > 0.0 && FBox2d(FVector2D::ZeroVector, Size).IsInsideOrOn(LocalCursorPosition))
		{
			if (Event.GetEffectingButton() == EKeys::MiddleMouseButton)
			{
				RequestMapPing(LocalCursorPosition / Size);
				return FReply::Handled();
			}
			if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
				return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply UUILevelMapWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
		return FReply::Handled().ReleaseMouseCapture();
	return Super::NativeOnMouseButtonUp(Geometry, Event);
}

FReply UUILevelMapWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!HasMouseCapture() || !MapUVRegion.bIsValid) return Super::NativeOnMouseMove(Geometry, Event);
	if (!Event.IsMouseButtonDown(EKeys::LeftMouseButton)) return FReply::Handled().ReleaseMouseCapture();

	const auto& ImageGeometry = MapImage->GetCachedGeometry();
	const FVector2D Size = ImageGeometry.GetLocalSize();
	if (Size.X > 0.0 && Size.Y > 0.0)
	{
		const FVector2D Current = ImageGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		const FVector2D Previous = ImageGeometry.AbsoluteToLocal(Event.GetLastScreenSpacePosition());
		PanMap((Current - Previous) / Size);
	}
	return FReply::Handled();
}

void UUILevelMapWidget::NativeDestruct()
{
	if (auto* Observer = MarkerObserver.Get())
	{
		Observer->OnMarkerVisible.RemoveAll(this);
		Observer->OnMarkerHidden.RemoveAll(this);
	}
	MarkerObserver.Reset();
	MapSource.Reset();
	MapUVRegion.Init();
	LocalPing = nullptr;
	for (const auto& Pair : MountedMarkers) Pair.Value->RemoveFromParent();
	MountedMarkers.Reset();
	Super::NativeDestruct();
}

void UUILevelMapWidget::MountOrUpdateMarker(UUIMarkerModel* Model, EUIMarkerSurface Surface, bool bPresentationChanged)
{
	if (Surface != EUIMarkerSurface::Map || !MapUVRegion.bIsValid) return;
	const auto* Source = MapSource.Get();
	auto* Player = GetOwningPlayer();
	if (!Source || !Player || !IsValid(Model)) return;
	const FGuid Id = Model->GetMarkerId();
	const FVector2D MapUV = Source->WorldToMapUV(Model->GetPosition());
	const bool bOutside = !MapUVRegion.IsInsideOrOn(MapUV);
	const auto* MarkerVisibility = Model->GetVisibility(Surface);
	if (MapUV.ContainsNaN() || (bOutside && (!MarkerVisibility || !MarkerVisibility->bKeepAtEdge)))
	{
		DemountMarker(Id, Surface);
		return;
	}
	auto* MarkerClass = Model->GetWidgetClass();
	auto* MarkerWidget = MountedMarkers.FindRef(Id).Get();
	UCanvasPanelSlot* CanvasSlot = nullptr;
	if (MarkerWidget && MarkerWidget->GetClass() == MarkerClass && MarkerWidget->GetOwningPlayer() == Player
		&& MarkerWidget->GetParent() == MarkerCanvas.Get())
	{
		if (bPresentationChanged) MarkerWidget->SetMarkerModel(Model);
		CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot);
	}
	else
	{
		DemountMarker(Id, Surface);
		CanvasSlot = UUIMarkerWidget::Mount(MarkerCanvas.Get(), Player, Model);
		if (!CanvasSlot) return;
		MountedMarkers.Add(Id, CastChecked<UUIMarkerWidget>(CanvasSlot->GetContent()));
		CanvasSlot->SetPosition(FVector2D::ZeroVector);
	}

	if (CanvasSlot)
	{
		const FVector2D ViewPosition = (MapUV - MapUVRegion.Min) / MapUVRegion.GetSize();
		const FVector2D Anchor = bOutside ? UUIMarkerWidget::GetEdgeAnchor(ViewPosition) : ViewPosition;
		FAnchors Anchors;
		Anchors.Minimum = Anchors.Maximum = Anchor;
		CanvasSlot->SetAnchors(Anchors);
		CanvasSlot->SetAlignment(bOutside ? Anchor : FVector2D(0.5, 0.5));
	}
}

void UUILevelMapWidget::DemountMarker(FGuid Id, EUIMarkerSurface Surface)
{
	if (Surface != EUIMarkerSurface::Map) return;
	TObjectPtr<UUIMarkerWidget> Widget;
	if (MountedMarkers.RemoveAndCopyValue(Id, Widget)) Widget->RemoveFromParent();
}
