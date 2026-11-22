#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "NGGameUserSettings.generated.h"

enum class ENGVolumeChannel : uint8;
class APlayerController;

UCLASS()
class GAMESETTINGS_API UNGGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static UNGGameUserSettings* Get();
	FVector2D ScaleLook(FVector2D Look, bool bGamepad, bool bAiming) const;
	FVector2D FilterMove(FVector2D Move) const;
	void ApplyForPlayer(APlayerController& Controller);
	float GetVolume(ENGVolumeChannel Channel) const;
	const FString& GetAudioOutputDeviceId() const { return AudioOutputDeviceId; }
	void RunStartupBenchmark();
	void LoadSettings(bool bForceReload = false) override;
	void ApplyNonResolutionSettings() override;
	float GetDashDirectionRegistrationDelay() const { return DashInputDirectionRegistrationDelay; }

private:
	UPROPERTY(Config) float DisplayGamma = 2.2f;
	UPROPERTY(Config) int32 MotionBlurQuality = 1;
	UPROPERTY(Config) int32 BloomQuality = 1;
	UPROPERTY(Config) int32 ColorBlindMode = 0;
	UPROPERTY(Config) int32 ColorBlindStrength = 0;
	UPROPERTY(Config) bool Subtitles = true;
	UPROPERTY(Config) bool BackgroundAudio = false;
	UPROPERTY(Config) FString AudioOutputDeviceId;
	UPROPERTY(Config) float OverallVolume = 1.f;
	UPROPERTY(Config) float MusicVolume = 1.f;
	UPROPERTY(Config) float SoundEffectsVolume = 1.f;
	UPROPERTY(Config) float UIVolume = 1.f;
	UPROPERTY(Config) float AmbientVolume = 1.f;
	UPROPERTY(Config) float DialogueVolume = 1.f;
	UPROPERTY(Config) bool GamepadVibration = true;
	UPROPERTY(Config) bool InvertVerticalAxis_Gamepad = false;
	UPROPERTY(Config) bool InvertHorizontalAxis_Gamepad = false;
	UPROPERTY(Config) int32 LookSensitivityPreset = 4;
	UPROPERTY(Config) int32 LookSensitivityPresetAds = 4;
	UPROPERTY(Config) float MoveStickDeadZone = 0.15f;
	UPROPERTY(Config) float LookStickDeadZone = 0.15f;
	UPROPERTY(Config) float MouseSensitivityYaw = 1.f;
	UPROPERTY(Config) float MouseSensitivityPitch = 1.f;
	UPROPERTY(Config) float DashInputDirectionRegistrationDelay = 0.f;
	UPROPERTY(Config) bool InvertVerticalAxis = false;
	UPROPERTY(Config) bool InvertHorizontalAxis = false;
	UPROPERTY(Config) bool bImportedInputPreferences = false;
};
