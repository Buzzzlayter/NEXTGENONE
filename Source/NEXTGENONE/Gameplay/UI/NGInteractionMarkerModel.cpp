#include "Gameplay/UI/NGInteractionMarkerModel.h"
#include "Gameplay/UI/NGInteractionWidget.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Input/PlayerInputSubsystem.h"

UNGInteractionMarkerModel::UNGInteractionMarkerModel()
{
	WorldVisibility.bAlwaysVisible = true;
}

void UNGInteractionMarkerModel::SetViewer(ULocalPlayer* Player)
{
	if (auto* Previous = LocalViewer.Get())
	{
		if (auto* Input = Previous->GetSubsystem<UPlayerInputSubsystem>()) Input->OnInputTypeChanged.RemoveDynamic(this, &ThisClass::InputTypeChanged);
		if (auto* Enhanced = Previous->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) Enhanced->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &ThisClass::RefreshPrompt);
	}
	LocalViewer = Player;
	if (!Player) return;
	if (auto* Input = Player->GetSubsystem<UPlayerInputSubsystem>()) Input->OnInputTypeChanged.AddUniqueDynamic(this, &ThisClass::InputTypeChanged);
	if (auto* Enhanced = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) Enhanced->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::RefreshPrompt);
}

void UNGInteractionMarkerModel::SetFocus(AActor* Target, const UInputAction* Action, const FText& Prompt, bool bAvailable)
{
	if (!IsValid(Target)) return;
	if (FocusTarget.Get() != Target)
	{
		FVector BoundsOrigin = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		Target->GetActorBounds(false, BoundsOrigin, BoundsExtent);
		LocalAnchor = Target->GetActorTransform().InverseTransformPosition(BoundsOrigin + FVector(0., 0., BoundsExtent.Z + 10.));
		FocusTarget = Target;
	}
	InputAction = Action;
	FocusPrompt = Prompt;
	bFocusAvailable = bAvailable;
	RefreshPrompt();
}

void UNGInteractionMarkerModel::InputTypeChanged(EInputType)
{
	RefreshPrompt();
}

void UNGInteractionMarkerModel::RefreshPrompt()
{
	FText NewLabel = FocusPrompt;
	if (!bFocusAvailable)
		NewLabel = FText::Format(NSLOCTEXT("NEXTGENONE", "InteractionPromptUnavailable", "{0} (unavailable)"), FocusPrompt);
	else if (auto* Player = LocalViewer.Get(); Player && InputAction.IsValid())
	{
		const auto* Enhanced = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		const auto* Input = Player->GetSubsystem<UPlayerInputSubsystem>();
		const bool bGamepad = Input && Input->IsGamepad();
		if (Enhanced)
		{
			const auto Keys = Enhanced->QueryKeysMappedToAction(InputAction.Get());
			if (const auto* Key = Keys.FindByPredicate([bGamepad](const FKey& Candidate) { return Candidate.IsGamepadKey() == bGamepad; }))
				NewLabel = FText::Format(NSLOCTEXT("NEXTGENONE", "InteractionMarkerKeyPrompt", "[{0}] {1}"), Key->GetDisplayName(false), FocusPrompt);
		}
	}
	if (PromptLabel.EqualTo(NewLabel)) return;
	PromptLabel = MoveTemp(NewLabel);
	NotifyMarkerChanged();
}

bool UNGInteractionMarkerModel::IsVisibleTo(const APlayerController& Viewer) const
{
	return FocusTarget.IsValid() && LocalViewer.IsValid() && Viewer.GetLocalPlayer() == LocalViewer.Get();
}

FVector UNGInteractionMarkerModel::GetPosition() const
{
	const auto* Target = FocusTarget.Get();
	return Target ? Target->GetActorTransform().TransformPosition(LocalAnchor) : Super::GetPosition();
}

const FUIMarkerVisibilityRules* UNGInteractionMarkerModel::GetVisibility(EUIMarkerSurface Surface) const
{
	return Surface == EUIMarkerSurface::World ? &WorldVisibility : nullptr;
}

UClass* UNGInteractionMarkerModel::GetWidgetClass() const
{
	return UNGInteractionWidget::StaticClass();
}
