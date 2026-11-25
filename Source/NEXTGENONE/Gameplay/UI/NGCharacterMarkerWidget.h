#pragma once

#include "Map/UIMarkerWidget.h"
#include "NGCharacterMarkerWidget.generated.h"

class UNGAbilitySetVitalsWidget;
class UTextBlock;

UCLASS(Blueprintable, PrioritizeCategories = ("Character Marker"))
class NEXTGENONE_API UNGCharacterMarkerWidget : public UUIMarkerWidget
{
	GENERATED_BODY()

public:
	virtual bool SupportsSurface(EUIMarkerSurface Surface) const override { return Surface == EUIMarkerSurface::World; }
	virtual TOptional<FVector2D> GetOwnMarkerScreenAnchor() const override { return OwnMarkerScreenAnchor; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, Category = "Character Marker")
	FVector2D OwnMarkerScreenAnchor = FVector2D(.5, .16);

private:
	void RefreshCharacter();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CharacterName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UNGAbilitySetVitalsWidget> Vitals;
	TOptional<FString> CachedPlayerName;
	float NextRefresh = 0.f;
};
