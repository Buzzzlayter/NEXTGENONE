#include "Map/UIMapCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/App.h"

AUIMapCapture::AUIMapCapture()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Map Camera"));
	Capture->SetupAttachment(GetRootComponent());
	Capture->SetRelativeLocationAndRotation(FVector(0.0, 0.0, 10000.0), FRotator(-90.0, 0.0, 0.0));
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->OrthoWidth = 20000.f;
	Capture->bAutoCalculateOrthoPlanes = true;
	Capture->bUpdateOrthoPlanes = false;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->ShowFlags.SetTemporalAA(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetDepthOfField(false);
	Capture->ShowFlags.SetEyeAdaptation(false);
	Capture->ShowFlags.SetBloom(false);
}

FVector2D AUIMapCapture::WorldToMapUV(const FVector& Position) const
{
	const FVector Offset = Position - Capture->GetComponentLocation();
	const double Width = Capture->OrthoWidth;
	return FVector2D(0.5 + (Offset | Capture->GetRightVector()) / Width,
		0.5 - (Offset | Capture->GetUpVector()) / Width);
}

FVector AUIMapCapture::MapUVToWorld(const FVector2D& MapUV) const
{
	const double Width = Capture->OrthoWidth;
	return Capture->GetComponentLocation()
		+ Capture->GetRightVector() * ((MapUV.X - 0.5) * Width)
		+ Capture->GetUpVector() * ((0.5 - MapUV.Y) * Width);
}

UTextureRenderTarget2D* AUIMapCapture::GetOrCaptureMap()
{
	if (MapRenderTarget) return MapRenderTarget.Get();
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || GetNetMode() == NM_DedicatedServer || !FApp::CanEverRender()) return nullptr;
	if (Capture->ProjectionType != ECameraProjectionMode::Orthographic || Capture->OrthoWidth <= 0.f
		|| Capture->GetForwardVector().Z > -0.9999 || Capture->bUseCustomProjectionMatrix)
	{
		UE_LOG(LogTemp, Warning, TEXT("Map capture %s requires a downward orthographic camera."), *GetName());
		return nullptr;
	}

	const int32 Resolution = FMath::Clamp(MapResolution, 128, 4096);
	MapRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Resolution, Resolution, RTF_RGBA8_SRGB);
	if (!MapRenderTarget) return nullptr;
	MapRenderTarget->SetFlags(RF_Transient);

	TGuardValue<TObjectPtr<UTextureRenderTarget2D>> CaptureTarget(Capture->TextureTarget, MapRenderTarget);
	Capture->CaptureScene();
	return MapRenderTarget.Get();
}
