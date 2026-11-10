#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/UIWidgetMounterComponent.h"
#include "NGSettingsPanel.generated.h"

class UComboBoxString;
class UCheckBox;
class USlider;
class UTextBlock;
class UNGConnectionPanel;
class UNGDebugSettingsPanel;

UCLASS(Config=GameUserSettings)
class NEXTGENONE_API UNGInputPreferences : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config) float LookSensitivity=1.f;
	UPROPERTY(Config) bool bInvertY=false;
	UPROPERTY(Config) float MasterVolume=1.f;
	void ApplyAudio(UWorld* World) const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNGSettingsBack);
UCLASS(Blueprintable)
class NEXTGENONE_API UNGSettingsPanel : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly,Category="Network UI") TSubclassOf<UNGConnectionPanel> ConnectionPanelClass;
	UPROPERTY(EditDefaultsOnly,Category="Debug UI") TSubclassOf<UNGDebugSettingsPanel> DebugPanelClass;
	UPROPERTY(EditDefaultsOnly, Category="Input") TArray<TObjectPtr<class UInputAction>> CloseActions;
	UPROPERTY(EditDefaultsOnly, Category="Input", meta=(MultiLine=true)) FText ControlsDescription;
	UPROPERTY(BlueprintAssignable) FNGSettingsBack OnBack;
	UFUNCTION(BlueprintCallable) void Apply();
	UFUNCTION(BlueprintCallable) void Cancel();
	UFUNCTION(BlueprintCallable) void Defaults();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> Quality;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> WindowMode;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> Resolution;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> FrameLimit;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> VSync;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> Invert;
	UPROPERTY(Transient) TObjectPtr<USlider> Sensitivity;
	UPROPERTY(Transient) TObjectPtr<USlider> Volume;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
	TArray<FIntPoint> Resolutions;
	void LoadValues();
};

UCLASS(EditInlineNew,DisplayName="Settings Screen")
class NEXTGENONE_API UNGSettingsScreenMounter : public UUIWidgetMounterComponent
{
	GENERATED_BODY()
public:
	UNGSettingsScreenMounter();
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
private:
	UFUNCTION() void Back();
	void Release();
	TWeakObjectPtr<class UPlayerInputSubsystem> AppliedInput;
	FGuid PolicyHandle;
};
