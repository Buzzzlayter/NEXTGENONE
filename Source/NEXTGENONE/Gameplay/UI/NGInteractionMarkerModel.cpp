#include "Gameplay/UI/NGInteractionMarkerModel.h"
#include "Engine/LocalPlayer.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Input/NGGameplayInputComponent.h"
#include "Input/NGGameplayInputConfig.h"
#include "Input/PlayerInputSubsystem.h"
#include "InputAction.h"

UNGInteractionMarkerModel::UNGInteractionMarkerModel()
{
	WorldVisibility.bAlwaysVisible = true;
}

void UNGInteractionMarkerModel::SetAnchor(USceneComponent* Component)
{
	AnchorComponent = Component;
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

void UNGInteractionMarkerModel::SetFocus(const APawn* Interactor, const FText& Prompt, bool bAvailable)
{
	const auto* GameplayInput = Interactor->FindComponentByClass<UNGGameplayInputComponent>();
	const auto* InputConfig = GameplayInput ? GameplayInput->InputConfig.Get() : nullptr;
	const auto* Action = InputConfig ? InputConfig->FindAbilityInputAction(TAG_NGInputInteract) : nullptr;
	if (!Action && InputConfig) Action = InputConfig->FindNativeInputAction(TAG_NGInputInteract);
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
	FKey MappedKey;
	if (auto* Player = LocalViewer.Get(); Player && bFocusAvailable && InputAction.IsValid())
	{
		const auto* Enhanced = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		const auto* Input = Player->GetSubsystem<UPlayerInputSubsystem>();
		const bool bGamepad = Input && Input->IsGamepad();
		if (Enhanced)
		{
			const auto Keys = Enhanced->QueryKeysMappedToAction(InputAction.Get());
			if (const auto* Key = Keys.FindByPredicate([bGamepad](const FKey& Candidate) { return Candidate.IsGamepadKey() == bGamepad; }))
				MappedKey = *Key;
		}
	}
	FText NewLabel = FormatPrompt(FocusPrompt, bFocusAvailable, MappedKey);
	if (PromptLabel.EqualTo(NewLabel)) return;
	PromptLabel = MoveTemp(NewLabel);
	NotifyMarkerChanged();
}

FText UNGInteractionMarkerModel::FormatPrompt_Implementation(const FText& Prompt, bool bAvailable, const FKey& MappedKey) const
{
	if (!bAvailable)
		return FText::Format(NSLOCTEXT("NEXTGENONE", "InteractionPromptUnavailable", "{0} (unavailable)"), Prompt);
	if (MappedKey.IsValid())
		return FText::Format(NSLOCTEXT("NEXTGENONE", "InteractionMarkerKeyPrompt", "[{0}] {1}"), MappedKey.GetDisplayName(false), Prompt);
	return Prompt;
}

bool UNGInteractionMarkerModel::IsVisibleTo(const APlayerController& Viewer) const
{
	return AnchorComponent.IsValid() && LocalViewer.IsValid() && Viewer.GetLocalPlayer() == LocalViewer.Get();
}

FVector UNGInteractionMarkerModel::GetPosition() const
{
	if (const auto* Component = AnchorComponent.Get()) return Component->GetComponentLocation();
	return Super::GetPosition();
}

const FUIMarkerVisibilityRules* UNGInteractionMarkerModel::GetVisibility(EUIMarkerSurface Surface) const
{
	return Surface == EUIMarkerSurface::World ? &WorldVisibility : nullptr;
}
