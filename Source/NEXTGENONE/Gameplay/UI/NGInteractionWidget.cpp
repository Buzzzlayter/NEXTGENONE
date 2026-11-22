#include "Gameplay/UI/NGInteractionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UNGInteractionWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		auto* Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetPadding(FMargin(10.f, 5.f));
		Panel->SetBrushColor(FLinearColor(0.025f, 0.025f, 0.025f, 0.9f));
		Panel->SetVisibility(ESlateVisibility::HitTestInvisible);

		MarkerLabel = WidgetTree->ConstructWidget<UTextBlock>();
		MarkerLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14));
		Panel->SetContent(MarkerLabel);
		WidgetTree->RootWidget = Panel;
	}
	return Super::RebuildWidget();
}
