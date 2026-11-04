#include "Gameplay/UI/NGInteractionPrompt.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UNGInteractionPrompt::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (auto* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::PawnChanged);
		PawnChanged(nullptr, PC->GetPawn());
	}
	else FocusChanged(nullptr, FText::GetEmpty(), false);
}

void UNGInteractionPrompt::PawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (Interaction.IsValid()) Interaction->OnFocusChanged.RemoveDynamic(this, &ThisClass::FocusChanged);
	Interaction = NewPawn ? NewPawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	if (Interaction.IsValid())
	{
		Interaction->OnFocusChanged.AddUniqueDynamic(this, &ThisClass::FocusChanged);
		FocusChanged(Interaction->GetFocusedActor(), Interaction->GetFocusedPrompt(), Interaction->CanUseFocusedActor());
	}
	else FocusChanged(nullptr, FText::GetEmpty(), false);
}

void UNGInteractionPrompt::FocusChanged(AActor* Target, FText Prompt, bool bAvailable)
{
	if (!InteractionText) return;
	InteractionText->SetVisibility(Target ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	InteractionText->SetText(Target ? FText::Format(bAvailable ? AvailableFormat : UnavailableFormat, Prompt) : FText::GetEmpty());
}

void UNGInteractionPrompt::NativeDestruct()
{
	if (Interaction.IsValid()) Interaction->OnFocusChanged.RemoveDynamic(this, &ThisClass::FocusChanged);
	if (auto* PC = GetOwningPlayer()) PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::PawnChanged);
	Interaction.Reset();
	Super::NativeDestruct();
}
