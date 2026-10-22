
#include "Components/ResolutionObserver.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "UnrealClient.h"


bool UResolutionObserver::Enter()
{
	const ULocalPlayer* Player = GetLocalPlayer();
	if (!ensureMsgf(Player && Player->ViewportClient, TEXT("Resolution observer has no owning player's viewport.")))
	{
		return false;
	}
	if (!Super::Enter())
	{
		return false;
	}
	ViewportResizedHandle = FViewport::ViewportResizedEvent.AddUObject(this, &ThisClass::ResolutionChanged);
	return true;
}
void UResolutionObserver::Exit(FName event)
{
	FViewport::ViewportResizedEvent.Remove(ViewportResizedHandle);
	ViewportResizedHandle.Reset();
	Super::Exit(event);
}

void UResolutionObserver::ResolutionChanged(FViewport* ViewPort, uint32 Val)
{
	const ULocalPlayer* Player = GetLocalPlayer();
	const UGameViewportClient* ViewportClient = Player ? Player->ViewportClient : nullptr;
	if (!ViewPort || !ViewportClient || ViewportClient->Viewport != ViewPort)
	{
		return;
	}
	const FIntPoint ViewportSize = ViewPort->GetSizeXY();
	const float DPIScale =
		GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));
	OnResolutionChanged.Broadcast(ViewportSize, DPIScale);
}
