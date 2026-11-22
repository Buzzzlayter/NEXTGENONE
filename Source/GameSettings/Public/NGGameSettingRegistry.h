#pragma once

#include "CoreMinimal.h"
#include "AudioMixerBlueprintLibrary.h"
#include "GameSettingRegistry.h"
#include "GameSettingRegistryChangeTracker.h"
#include "NGGameSettingRegistry.generated.h"

class UGameSettingCollection;
class UGameSettingValueDiscreteDynamic;
class UNGGameUserSettings;

UCLASS()
class GAMESETTINGS_API UNGGameSettingRegistry : public UGameSettingRegistry
{
	GENERATED_BODY()

public:
	const TArray<TObjectPtr<UGameSetting>>& GetSections() const { return TopLevelSettings; }
	void SaveChanges() override;
	void RevertChanges();
	void ResetToDefaults();
	bool HasChanges() const { return ChangeTracker.HaveSettingsBeenChanged(); }

protected:
	void OnInitialize(ULocalPlayer* InLocalPlayer) override;

private:
	void BuildVideo(UGameSettingCollection& Section);
	void BuildAudio(UGameSettingCollection& Section);
	void BuildMouseAndKeyboard(UGameSettingCollection& Section);
	void BuildGamepad(UGameSettingCollection& Section);
	void BuildGameplay(UGameSettingCollection& Section);
	UGameSettingCollection& AddSection(FName Name, const FText& Label);
	void CommitMachineSettings(UNGGameUserSettings& LiveSettings);
	UFUNCTION() void OnAudioDevicesEnumerated(const TArray<FAudioOutputDeviceInfo>& Devices);

	UPROPERTY(Transient) TObjectPtr<UNGGameUserSettings> Baseline;
	UPROPERTY(Transient) TObjectPtr<UNGGameUserSettings> Draft;
	UPROPERTY(Transient) TObjectPtr<UGameSettingValueDiscreteDynamic> AudioDevice;
	FString InitialCulture;
	FString SelectedCulture;
	FGameSettingRegistryChangeTracker ChangeTracker;
};
