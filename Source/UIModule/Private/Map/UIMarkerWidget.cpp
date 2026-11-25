#include "Map/UIMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Map/UIMarkerModel.h"
#include "Styling/CoreStyle.h"

UCanvasPanelSlot* UUIMarkerWidget::Mount(UCanvasPanel* Canvas, APlayerController* Player, UUIMarkerModel* Model, EUIMarkerSurface Surface)
{
	auto* Widget = CreateWidget<UUIMarkerWidget>(Player, Model->GetWidgetClass(Surface));
	if (!Widget) return nullptr;

	Widget->SetMarkerModel(Model);
	auto* CanvasSlot = Canvas->AddChildToCanvas(Widget);
	if (CanvasSlot)
	{
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAlignment(FVector2D(0.5, 0.5));
	}
	return CanvasSlot;
}

FVector2D UUIMarkerWidget::GetEdgeAnchor(const FVector2D& ViewPosition)
{
	const FVector2D Center(0.5, 0.5);
	const FVector2D Direction = ViewPosition - Center;
	const double Extent = Direction.GetAbsMax();
	return Extent > UE_KINDA_SMALL_NUMBER ? Center + Direction * (0.5 / Extent) : FVector2D(0.5, 1.0);
}

void UUIMarkerWidget::SetMarkerModel(UUIMarkerModel* Model)
{
	MarkerModel = Model;
	if (GetCachedWidget().IsValid()) RefreshPresentation();
}

void UUIMarkerWidget::GetModel(TSubclassOf<UUIMarkerModel> ModelClass, UUIMarkerModel*& OutModel)
{
	OutModel = IsValid(MarkerModel) && (!ModelClass || MarkerModel->IsA(ModelClass.Get())) ? MarkerModel.Get() : nullptr;
}

TSharedRef<SWidget> UUIMarkerWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		MarkerIcon = WidgetTree->ConstructWidget<UImage>();
		MarkerIcon->SetDesiredSizeOverride(FVector2D(16.0, 16.0));
		Row->AddChildToHorizontalBox(MarkerIcon)->SetVerticalAlignment(VAlign_Center);
		MarkerLabel = WidgetTree->ConstructWidget<UTextBlock>();
		MarkerLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10.f));
		auto* LabelSlot = Row->AddChildToHorizontalBox(MarkerLabel);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
		WidgetTree->RootWidget = Row;
	}
	return Super::RebuildWidget();
}

void UUIMarkerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshPresentation();
}

void UUIMarkerWidget::RefreshPresentation()
{
	if (!IsValid(MarkerModel)) return;
	if (MarkerIcon)
	{
		if (auto* Icon = MarkerModel->GetIcon()) MarkerIcon->SetBrushFromTexture(Icon);
		else MarkerIcon->SetBrush(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
	}
	if (MarkerLabel)
	{
		const FText& Label = MarkerModel->GetLabel();
		MarkerLabel->SetText(Label);
		MarkerLabel->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	OnMarkerUpdated();
}
