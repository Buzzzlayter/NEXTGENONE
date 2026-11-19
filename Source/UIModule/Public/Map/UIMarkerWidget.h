#pragma once

#include "Blueprint/UserWidget.h"
#include "Misc/EnumRange.h"
#include "UIMarkerWidget.generated.h"

class APlayerController;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UTextBlock;
class UUIMarkerModel;

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EUIMarkerSurface : uint8
{
	None = 0 UMETA(DisplayName = "None", Hidden),
	Map = 1 UMETA(DisplayName = "Map"),
	World = 2 UMETA(DisplayName = "World Canvas"),
	Minimap = 4 UMETA(DisplayName = "Minimap")
};

ENUM_RANGE_BY_VALUES(EUIMarkerSurface, EUIMarkerSurface::Map, EUIMarkerSurface::World, EUIMarkerSurface::Minimap);

UCLASS(Blueprintable, PrioritizeCategories = ("Marker"))
class UIMODULE_API UUIMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static UCanvasPanelSlot* Mount(UCanvasPanel* Canvas, APlayerController* Player, UUIMarkerModel* Model);
	static FVector2D GetEdgeAnchor(const FVector2D& ViewPosition);
	void SetMarkerModel(UUIMarkerModel* Model);

	UFUNCTION(BlueprintCallable, Category = "Marker", meta = (DeterminesOutputType = "ModelClass", DynamicOutputParam = "OutModel", CompactNodeTitle = "GetModel"))
	void GetModel(TSubclassOf<UUIMarkerModel> ModelClass, UUIMarkerModel*& OutModel);

	bool SupportsSurface(EUIMarkerSurface Surface) const
	{
		return (Surfaces & static_cast<int32>(Surface)) != 0;
	}

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Marker")
	void OnMarkerUpdated();

private:
	void RefreshPresentation();

	UPROPERTY(EditDefaultsOnly, Category = "Marker", meta = (Bitmask, BitmaskEnum = "/Script/UIModule.EUIMarkerSurface"))
	int32 Surfaces = static_cast<int32>(EUIMarkerSurface::Map)
		| static_cast<int32>(EUIMarkerSurface::World)
		| static_cast<int32>(EUIMarkerSurface::Minimap);

	UPROPERTY(Transient)
	TObjectPtr<UUIMarkerModel> MarkerModel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> MarkerIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MarkerLabel;
};
