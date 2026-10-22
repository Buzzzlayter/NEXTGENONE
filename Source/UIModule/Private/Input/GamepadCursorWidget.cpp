// Exclusive publisher - Forward Gateway, LLC

#include "Input/GamepadCursorWidget.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/UserInterfaceSettings.h"
#include "UnrealClient.h"

void UGamepadCursorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ViewportResizedHandle = FViewport::ViewportResizedEvent.AddUObject(this, &ThisClass::ViewportResized);
	if (const ULocalPlayer* Player = GetOwningLocalPlayer())
	{
		if (const UGameViewportClient* ViewportClient = Player->ViewportClient)
		{
			ViewportResized(ViewportClient->Viewport, 0);
		}
	}
}

void UGamepadCursorWidget::NativeDestruct()
{
	FViewport::ViewportResizedEvent.Remove(ViewportResizedHandle);
	ViewportResizedHandle.Reset();
	Super::NativeDestruct();
}

void UGamepadCursorWidget::ViewportResized(FViewport* Viewport, uint32 Value)
{
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	if (Viewport && Player && Player->ViewportClient && Player->ViewportClient->Viewport == Viewport)
	{
		OnViewportResized(GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Viewport->GetSizeXY()));
	}
}
