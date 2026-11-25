#pragma once

#include "Components/UIWidgetMounterComponent.h"
#include "NGGameSettingsScreen.h"
#include "NGSettingsPanel.generated.h"

class UInputAction;
class UPlayerInputSubsystem;

UCLASS(Blueprintable)
class NEXTGENONE_API UNGSettingsPanel : public UNGGameSettingsScreen
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TArray<TObjectPtr<UInputAction>> CloseActions;

protected:
	FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
};

UCLASS(EditInlineNew, DisplayName="Settings Screen")
class NEXTGENONE_API UNGSettingsScreenMounter : public UUIWidgetMounterComponent
{
	GENERATED_BODY()

public:
	UNGSettingsScreenMounter();
	void Enter() override;
	void Exit(FName Event) override;

private:
	TWeakObjectPtr<UPlayerInputSubsystem> AppliedInput;
	FGuid PolicyHandle;
};
