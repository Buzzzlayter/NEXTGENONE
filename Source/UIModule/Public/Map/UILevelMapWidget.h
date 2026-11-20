#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UILevelMapWidget.generated.h"

class AUIMapCapture;
class UCanvasPanel;
class UImage;
class UUIMarkerModel;
class UUIMarkerObserver;
class UUIMarkerWidget;
enum class EUIMarkerSurface : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FUIMapPingRequested, const FVector&);

UCLASS(Abstract, Blueprintable, PrioritizeCategories = ("Map"))
class UIMODULE_API UUILevelMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Map")
	void PanMap(FVector2D ViewDelta);

	UFUNCTION(BlueprintCallable, Category = "Map")
	void ZoomMap(double ZoomDelta);

	void RequestMapPingAtCenter();
	FUIMapPingRequested OnMapPingRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	void RequestMapPing(const FVector2D& ViewPosition);
	void ZoomMapAt(double ZoomDelta, const FVector2D& ViewAnchor);
	void UpdateMapView();
	void MountOrUpdateMarker(UUIMarkerModel* Model, EUIMarkerSurface Surface, bool bPresentationChanged);
	void DemountMarker(FGuid Id, EUIMarkerSurface Surface);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> MapImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> MarkerCanvas;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Map")
	TObjectPtr<UUIMarkerModel> LocalPingTemplate;

	UPROPERTY(Transient)
	TObjectPtr<UUIMarkerModel> LocalPing;

	TWeakObjectPtr<AUIMapCapture> MapSource;
	TWeakObjectPtr<UUIMarkerObserver> MarkerObserver;
	FBox2d MapUVRegion{ForceInit};
	FVector2D MapCenterUV{0.5, 0.5};
	double Zoom = 1.0;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UUIMarkerWidget>> MountedMarkers;
};
