#pragma once

#include "Blueprint/UserWidget.h"
#include "NGGameSettingsScreen.generated.h"

class UNGGameSettingRegistry;
class UGameSetting;
class SVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNGSettingsBack);

UCLASS(Blueprintable)
class GAMESETTINGS_API UNGGameSettingsScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category="Settings")
	FNGSettingsBack OnBack;

	UFUNCTION(BlueprintCallable, Category="Settings")
	void Apply();

	UFUNCTION(BlueprintCallable, Category="Settings")
	void Cancel();

	UFUNCTION(BlueprintCallable, Category="Settings")
	void Defaults();

	UNGGameSettingRegistry* GetRegistry() const { return Registry; }

protected:
	TSharedRef<SWidget> RebuildWidget() override;
	void NativeConstruct() override;
	void NativeDestruct() override;
	void ReleaseSlateResources(bool bReleaseChildren) override;
	FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	void AddSetting(SVerticalBox& Body, UGameSetting& Setting);
	void RefreshSettings();

	UPROPERTY(Transient)
	TObjectPtr<UNGGameSettingRegistry> Registry;

	TSharedPtr<SVerticalBox> SettingsRows;
};
