#pragma once

#include "CoreMinimal.h"
#include "NGGameSettingsConfig.h"
#include "Subsystems/WorldSubsystem.h"
#include "NGAudioMixEffectsSubsystem.generated.h"

class FAudioDevice;
class UNGGameUserSettings;
class USoundClass;
class USoundControlBus;
class USoundControlBusMix;
class USoundMix;

UCLASS()
class GAMESETTINGS_API UNGAudioMixEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void ApplyUserSettings(const UNGGameUserSettings& Settings, bool bApplyOutputDevice);

protected:
	bool ShouldCreateSubsystem(UObject* Outer) const override;
	bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	void OnWorldBeginPlay(UWorld& World) override;
	void OnWorldEndPlay(UWorld& World) override;
	void Deinitialize() override;

private:
	void InitializeMixes(FAudioDevice& Device);
	void UpdateBusMix(const UNGGameUserSettings& Settings);
	void ReleaseMixes();

	UPROPERTY(Transient)
	TMap<ENGVolumeChannel, TObjectPtr<USoundControlBus>> Buses;

	UPROPERTY(Transient)
	TMap<ENGVolumeChannel, TObjectPtr<USoundClass>> SoundClasses;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> UserMix;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> SoundClassMix;

	bool bInitializedMixes = false;
	bool bUserMixActive = false;
	bool bSoundClassMixActive = false;
};
