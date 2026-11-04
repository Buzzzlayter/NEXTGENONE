#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NGInteractionPrompt.generated.h"

class UNGInteractionComponent;
class UTextBlock;

/** Compact interaction label; placement belongs to the UI layout. */
UCLASS(Abstract, Blueprintable)
class NEXTGENONE_API UNGInteractionPrompt : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Interaction")
	TObjectPtr<UTextBlock> InteractionText;

	/** {0} is the focused object's prompt. Add a key hint in the widget's defaults if desired. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Interaction")
	FText AvailableFormat = NSLOCTEXT("NEXTGENONE", "InteractionPromptAvailable", "{0}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Interaction")
	FText UnavailableFormat = NSLOCTEXT("NEXTGENONE", "InteractionPromptUnavailable", "{0} (unavailable)");
private:
	TWeakObjectPtr<UNGInteractionComponent> Interaction;
	UFUNCTION() void PawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void FocusChanged(AActor* Target, FText Prompt, bool bAvailable);
};
