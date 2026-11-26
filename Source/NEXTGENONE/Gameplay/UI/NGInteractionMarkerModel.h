#pragma once

#include "Input/InputData.h"
#include "InputCoreTypes.h"
#include "Map/UIMarkerModel.h"
#include "NGInteractionMarkerModel.generated.h"

class ULocalPlayer;
class UInputAction;
class APawn;
class USceneComponent;

UCLASS(Blueprintable)
class UNGInteractionMarkerModel : public UUIMarkerModel
{
	GENERATED_BODY()

public:
	UNGInteractionMarkerModel();
	void SetViewer(ULocalPlayer* Player);
	void SetAnchor(USceneComponent* Component);
	void SetFocus(const APawn* Interactor, const FText& Prompt, bool bAvailable);
	virtual bool ShouldReplicateMarker() const override { return false; }
	virtual bool ShouldRegisterOnBeginPlay() const override { return false; }
	virtual bool UsesWorldWidgetComponent() const override { return true; }
	virtual bool IsVisibleTo(const APlayerController& Viewer) const override;
	virtual FVector GetPosition() const override;
	virtual const FText& GetLabel() const override { return PromptLabel; }
	virtual const FUIMarkerVisibilityRules* GetVisibility(EUIMarkerSurface Surface) const override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Marker")
	FText FormatPrompt(const FText& Prompt, bool bAvailable, const FKey& MappedKey) const;

private:
	UFUNCTION() void RefreshPrompt();
	UFUNCTION() void InputTypeChanged(EInputType InputType);
	TWeakObjectPtr<ULocalPlayer> LocalViewer;
	TWeakObjectPtr<USceneComponent> AnchorComponent;
	TWeakObjectPtr<const UInputAction> InputAction;
	FText FocusPrompt;
	FText PromptLabel;
	bool bFocusAvailable = false;
};
