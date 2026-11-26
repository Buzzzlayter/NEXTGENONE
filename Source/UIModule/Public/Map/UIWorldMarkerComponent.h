#pragma once

#include "Components/WidgetComponent.h"
#include "UIWorldMarkerComponent.generated.h"

class APlayerController;
class UUIMarkerModel;
class UUIMarkerWidget;

UCLASS(NotBlueprintable)
class UIMODULE_API UUIWorldMarkerComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UUIWorldMarkerComponent();

	virtual void InitWidget() override;
	bool ShowMarker(APlayerController* Viewer, UUIMarkerModel* Model, TSubclassOf<UUIMarkerWidget> PresentationClass);
	UUIMarkerModel* GetMarkerModel() const { return ActiveMarker.Get(); }

protected:
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	void MarkerChanged(UUIMarkerModel* Model, bool bPresentationChanged);
	void UpdateWidgetTransform(const APlayerController& Viewer);

	UPROPERTY(Transient)
	TObjectPtr<UUIMarkerModel> ActiveMarker;
};
