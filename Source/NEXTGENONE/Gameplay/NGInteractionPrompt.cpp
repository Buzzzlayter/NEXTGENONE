#include "NGInteractionPrompt.h"
#include "NGInteraction.h"
#include "NGAimComponent.h"
#include "NextgenoneCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"

TSharedRef<SWidget> UNGInteractionPrompt::RebuildWidget()
{
	if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	Label=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("InteractionText")));
	if (!WidgetTree->RootWidget)
	{
		auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
		Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("InteractionText"));
		auto* PromptSlot=Canvas->AddChildToCanvas(Label); PromptSlot->SetAnchors(FAnchors(.5f,.65f)); PromptSlot->SetAlignment(FVector2D(.5f,.5f)); PromptSlot->SetAutoSize(true);
	}
	AimDot=Cast<UImage>(WidgetTree->FindWidget(TEXT("AimDot")));
	if (!AimDot)
	{
		auto* Canvas=Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (!Canvas)
		{
			// Keep an authored prompt tree intact and add the reticle as an overlay.
			auto* Previous=WidgetTree->RootWidget.Get();
			auto* Overlay=WidgetTree->ConstructWidget<UOverlay>();
			auto* PreviousSlot=Overlay->AddChildToOverlay(Previous); PreviousSlot->SetHorizontalAlignment(HAlign_Fill); PreviousSlot->SetVerticalAlignment(VAlign_Fill);
			Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();
			auto* CanvasSlot=Overlay->AddChildToOverlay(Canvas); CanvasSlot->SetHorizontalAlignment(HAlign_Fill); CanvasSlot->SetVerticalAlignment(VAlign_Fill); WidgetTree->RootWidget=Overlay;
		}
		AimDot=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("AimDot"));
		AimDot->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White,2.5f));
		auto* DotSlot=Canvas->AddChildToCanvas(AimDot); DotSlot->SetAnchors(FAnchors(.5f,.5f)); DotSlot->SetAlignment(FVector2D(.5f,.5f)); DotSlot->SetSize(FVector2D(5.f));
		AimDot->SetVisibility(ESlateVisibility::Collapsed);
	}
	return Super::RebuildWidget();
}
void UNGInteractionPrompt::NativeConstruct()
{
	Super::NativeConstruct(); SetVisibility(ESlateVisibility::HitTestInvisible);
	if (auto* PC=GetOwningPlayer()) { PC->OnPossessedPawnChanged.AddUniqueDynamic(this,&ThisClass::PawnChanged); PawnChanged(nullptr,PC->GetPawn()); }
}
void UNGInteractionPrompt::PawnChanged(APawn* OldPawn,APawn* NewPawn)
{
	if (Interaction.IsValid()) Interaction->OnFocusChanged.RemoveDynamic(this,&ThisClass::FocusChanged);
	const auto* Pawn=Cast<ANextgenoneCharacter>(NewPawn); Interaction=Pawn ? Pawn->Interaction.Get() : nullptr; Aim=Pawn ? Pawn->Aim.Get() : nullptr;
	if (Interaction.IsValid())
	{
		Interaction->OnFocusChanged.AddUniqueDynamic(this,&ThisClass::FocusChanged);
		FocusChanged(Interaction->GetFocusedActor(),Interaction->GetFocusedPrompt(),Interaction->CanUseFocusedActor());
	}
	else FocusChanged(nullptr,FText::GetEmpty(),false);
}
void UNGInteractionPrompt::NativeTick(const FGeometry& Geometry,float Delta)
{
	Super::NativeTick(Geometry,Delta);
	if (!AimDot) return;
	const bool bVisible=Aim.IsValid() && Aim->ShouldShowReticle();
	AimDot->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		AimDot->SetColorAndOpacity(Aim->GetReticleColor());
		if (auto* DotSlot=Cast<UCanvasPanelSlot>(AimDot->Slot)) DotSlot->SetSize(FVector2D(FMath::Clamp(Aim->ReticleSize,2.f,16.f)));
	}
}
void UNGInteractionPrompt::FocusChanged(AActor* Target,FText Prompt,bool bAvailable)
{
	if (!Label) return;
	Label->SetVisibility(Target ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Label->SetText(FText::Format(bAvailable ? NSLOCTEXT("NEXTGENONE","InteractAvailable","E — {0}") : NSLOCTEXT("NEXTGENONE","InteractUnavailable","{0} (недоступно)"),Prompt));
}
void UNGInteractionPrompt::NativeDestruct()
{
	if (Interaction.IsValid()) Interaction->OnFocusChanged.RemoveDynamic(this,&ThisClass::FocusChanged);
	if (auto* PC=GetOwningPlayer()) PC->OnPossessedPawnChanged.RemoveDynamic(this,&ThisClass::PawnChanged);
	Interaction.Reset(); Aim.Reset(); Super::NativeDestruct();
}
UNGInteractionPromptMounter::UNGInteractionPromptMounter()
{
	WidgetClass=UNGInteractionPrompt::StaticClass();
}
