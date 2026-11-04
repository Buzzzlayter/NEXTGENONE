#include "Gameplay/UI/NGReticleWidget.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UNGReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (auto* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::PawnChanged);
		PawnChanged(nullptr, PC->GetPawn());
	}
	else UpdateReticle();
}

void UNGReticleWidget::PawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	Aim = NewPawn ? NewPawn->FindComponentByClass<UNGAimComponent>() : nullptr;
	UpdateReticle();
}

void UNGReticleWidget::NativeTick(const FGeometry& Geometry, float Delta)
{
	Super::NativeTick(Geometry, Delta);
	UpdateReticle();
}

void UNGReticleWidget::UpdateReticle()
{
	if (!AimDot) return;
	const bool bVisible = Aim.IsValid() && Aim->ShouldShowReticle();
	// Keep the UserWidget ticking so the dot can become visible again.
	AimDot->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (bVisible) AimDot->SetColorAndOpacity(Aim->GetReticleColor());
}

void UNGReticleWidget::NativeDestruct()
{
	if (auto* PC = GetOwningPlayer()) PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::PawnChanged);
	Aim.Reset();
	Super::NativeDestruct();
}
