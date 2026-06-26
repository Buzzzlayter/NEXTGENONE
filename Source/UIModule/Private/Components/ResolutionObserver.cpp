
#include "Components/ResolutionObserver.h"
#include "Engine/UserInterfaceSettings.h"


void UResolutionObserver::Enter()
{
	Super::Enter();
	GEngine->GameViewport->Viewport->ViewportResizedEvent.AddUObject(this, &ThisClass::ResolutionChanged);
}
void UResolutionObserver::Exit(FName event)
{
	GEngine->GameViewport->Viewport->ViewportResizedEvent.RemoveAll(this);
	Super::Exit(event);
}

void UResolutionObserver::ResolutionChanged(FViewport* ViewPort, uint32 Val)
{
	const FIntPoint ViewportSize = ViewPort->GetSizeXY();
	const float DPIScale =
		GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));
	OnResolutionChanged.Broadcast(ViewportSize, DPIScale);
}
