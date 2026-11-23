#pragma once

#include "Input/InputData.h"
#include "Map/UIMarkerModel.h"
#include "NGInteractionMarkerModel.generated.h"

class ULocalPlayer;
class UInputAction;
class AActor;
class USceneComponent;

UCLASS()
class UNGInteractionMarkerModel : public UUIMarkerModel
{
	GENERATED_BODY()

public:
	UNGInteractionMarkerModel();
	void SetViewer(ULocalPlayer* Player);
	void SetAnchor(USceneComponent* Component);
	void SetFocus(AActor* Target, const UInputAction* Action, const FText& Prompt, bool bAvailable);
	virtual bool ShouldReplicateMarker() const override { return false; }
	virtual bool ShouldRegisterOnBeginPlay() const override { return false; }
	virtual bool IsVisibleTo(const APlayerController& Viewer) const override;
	virtual FVector GetPosition() const override;
	virtual const FText& GetLabel() const override { return PromptLabel; }
	virtual const FUIMarkerVisibilityRules* GetVisibility(EUIMarkerSurface Surface) const override;

private:
	UFUNCTION() void RefreshPrompt();
	UFUNCTION() void InputTypeChanged(EInputType InputType);
	TWeakObjectPtr<ULocalPlayer> LocalViewer;
	TWeakObjectPtr<AActor> FocusTarget;
	TWeakObjectPtr<USceneComponent> AnchorComponent;
	TWeakObjectPtr<const UInputAction> InputAction;
	FVector LocalAnchor = FVector::ZeroVector;
	FText FocusPrompt;
	FText PromptLabel;
	bool bFocusAvailable = false;
};
