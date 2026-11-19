#pragma once

#include "Blueprint/UserWidget.h"
#include "UIWorldMarkersCanvas.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UUIMarkerModel;
class UUIMarkerObserver;
enum class EUIMarkerSurface : uint8;

UCLASS(Abstract, Blueprintable)
class UIMODULE_API UUIWorldMarkersCanvas : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void TrackMarker(UUIMarkerModel* Model, EUIMarkerSurface Surface, bool bPresentationChanged);
	void UntrackMarker(FGuid Id, EUIMarkerSurface Surface);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> RootCanvas;

	struct FTrackedMarker
	{
		TWeakObjectPtr<UUIMarkerModel> Model;
		TWeakObjectPtr<UCanvasPanelSlot> CanvasSlot;
	};

	static void DemountWidget(FTrackedMarker& Marker);

	TWeakObjectPtr<UUIMarkerObserver> MarkerObserver;
	TMap<FGuid, FTrackedMarker> TrackedMarkers;
};
