#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UIMapCapture.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UCLASS(PrioritizeCategories = ("Map"))
class UIMODULE_API AUIMapCapture : public AActor
{
	GENERATED_BODY()

public:
	AUIMapCapture();
	UTextureRenderTarget2D* GetOrCaptureMap();
	FVector2D WorldToMapUV(const FVector& Position) const;
	FVector MapUVToWorld(const FVector2D& MapUV) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Map")
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(EditAnywhere, Category = "Map", meta = (ClampMin = "128", ClampMax = "4096"))
	int32 MapResolution = 1024;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MapRenderTarget;
};
