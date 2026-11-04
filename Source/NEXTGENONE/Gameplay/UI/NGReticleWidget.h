#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NGReticleWidget.generated.h"

class UNGAimComponent;
class UImage;

/** Compact reticle; brush, dimensions and placement are authored in UMG/layout. */
UCLASS(Abstract, Blueprintable)
class NEXTGENONE_API UNGReticleWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float Delta) override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Aim")
	TObjectPtr<UImage> AimDot;
private:
	TWeakObjectPtr<UNGAimComponent> Aim;
	UFUNCTION() void PawnChanged(APawn* OldPawn, APawn* NewPawn);
	void UpdateReticle();
};
