#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/UIWidgetMounterComponent.h"
#include "NGInteractionPrompt.generated.h"

class UNGInteractionComponent;
class UTextBlock;
class UImage;
class UNGAimComponent;

UCLASS(Blueprintable)
class NEXTGENONE_API UNGInteractionPrompt : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
	UPROPERTY(Transient) TObjectPtr<UImage> AimDot;
	TWeakObjectPtr<UNGInteractionComponent> Interaction;
	TWeakObjectPtr<UNGAimComponent> Aim;
	UFUNCTION() void PawnChanged(APawn* OldPawn,APawn* NewPawn);
	UFUNCTION() void FocusChanged(AActor* Target,FText Prompt,bool bAvailable);
};

UCLASS(Blueprintable, EditInlineNew, DisplayName="Interaction Prompt")
class NEXTGENONE_API UNGInteractionPromptMounter : public UUIWidgetMounterComponent
{
	GENERATED_BODY()
public:
	UNGInteractionPromptMounter();
};
