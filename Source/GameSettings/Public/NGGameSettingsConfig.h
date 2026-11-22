#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NGGameSettingsConfig.generated.h"

class USoundClass;
class USoundControlBus;
class USoundControlBusMix;

UENUM()
enum class ENGVolumeChannel : uint8
{
	Overall,
	Music,
	Effects,
	UI,
	Ambient,
	Dialogue
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Settings"))
class GAMESETTINGS_API UNGGameSettingsConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="Registry")
	TSet<FName> HiddenSettings;

	UPROPERTY(Config, EditAnywhere, Category="Video")
	bool bEnableHardwareBenchmark = true;

	UPROPERTY(Config, EditAnywhere, Category="Video")
	bool bRunHardwareBenchmarkOnFirstLaunch = false;

	UPROPERTY(Config, EditAnywhere, Category="Input")
	TArray<float> GamepadSensitivity = { .5f, .75f, .9f, 1.f, 1.1f, 1.25f, 1.5f, 1.75f, 2.f, 2.5f };

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TMap<ENGVolumeChannel, TSoftObjectPtr<USoundControlBus>> VolumeBuses;

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundControlBusMix> UserSettingsControlBusMix;

	UPROPERTY(Config, EditAnywhere, Category="Audio", meta=(ToolTip="Sound class used for a channel without a control bus. Overall uses device volume. Effects defaults to the project's default sound class."))
	TMap<ENGVolumeChannel, TSoftObjectPtr<USoundClass>> VolumeSoundClasses;
};
