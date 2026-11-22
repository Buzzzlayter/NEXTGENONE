#include "NGAudioMixEffectsSubsystem.h"

#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioModulationStatics.h"
#include "Engine/World.h"
#include "NGGameSettingsConfig.h"
#include "NGGameUserSettings.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"

bool UNGAudioMixEffectsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UNGAudioMixEffectsSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNGAudioMixEffectsSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if (const UNGGameUserSettings* Settings = UNGGameUserSettings::Get())
	{
		ApplyUserSettings(*Settings, !Settings->GetAudioOutputDeviceId().IsEmpty());
	}
}

void UNGAudioMixEffectsSubsystem::OnWorldEndPlay(UWorld& World)
{
	ReleaseMixes();
	Super::OnWorldEndPlay(World);
}

void UNGAudioMixEffectsSubsystem::Deinitialize()
{
	ReleaseMixes();
	Super::Deinitialize();
}

void UNGAudioMixEffectsSubsystem::InitializeMixes(FAudioDevice& Device)
{
	const auto* Config = GetDefault<UNGGameSettingsConfig>();
	if (Device.IsModulationPluginEnabled())
	{
		for (const auto& Entry : Config->VolumeBuses)
		{
			if (USoundControlBus* Bus = Entry.Value.LoadSynchronous())
			{
				Buses.Add(Entry.Key, Bus);
			}
		}

		if (USoundControlBusMix* Template = Config->UserSettingsControlBusMix.LoadSynchronous())
		{
			UserMix = DuplicateObject<USoundControlBusMix>(Template, this);
			UserMix->Duration = -1.0;
			UserMix->bRetriggerOnActivation = false;
		}
		else if (!Buses.IsEmpty())
		{
			UserMix = NewObject<USoundControlBusMix>(this);
		}
	}

	for (const auto& Entry : Config->VolumeSoundClasses)
	{
		if (Entry.Key != ENGVolumeChannel::Overall && !Buses.Contains(Entry.Key))
		{
			if (USoundClass* SoundClass = Entry.Value.LoadSynchronous())
			{
				SoundClasses.Add(Entry.Key, SoundClass);
			}
		}
	}

	if (!Buses.Contains(ENGVolumeChannel::Effects) && !SoundClasses.Contains(ENGVolumeChannel::Effects))
	{
		if (USoundClass* DefaultSoundClass = GetDefault<UAudioSettings>()->GetDefaultSoundClass())
		{
			SoundClasses.Add(ENGVolumeChannel::Effects, DefaultSoundClass);
		}
	}

	if (!SoundClasses.IsEmpty())
	{
		SoundClassMix = NewObject<USoundMix>(this);
		SoundClassMix->FadeInTime = 0.f;
		SoundClassMix->FadeOutTime = 0.f;
	}
	bInitializedMixes = true;
}

void UNGAudioMixEffectsSubsystem::ApplyUserSettings(const UNGGameUserSettings& Settings, bool bApplyOutputDevice)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !World->AllowAudioPlayback())
	{
		return;
	}

	FAudioDeviceHandle Device = World->GetAudioDevice();
	if (!Device)
	{
		return;
	}

	if (!bInitializedMixes)
	{
		InitializeMixes(*Device.GetAudioDevice());
	}

	Device->SetTransientPrimaryVolume(Buses.Contains(ENGVolumeChannel::Overall)
		? 1.f : Settings.GetVolume(ENGVolumeChannel::Overall));

	UpdateBusMix(Settings);

	if (SoundClassMix)
	{
		for (const auto& Entry : SoundClasses)
		{
			Device->SetSoundMixClassOverride(SoundClassMix, Entry.Value, Settings.GetVolume(Entry.Key), 1.f, .05f, false);
		}
		if (!bSoundClassMixActive)
		{
			Device->PushSoundMixModifier(SoundClassMix);
			bSoundClassMixActive = true;
		}
	}

	if (bApplyOutputDevice)
	{
		UAudioMixerBlueprintLibrary::SwapAudioOutputDevice(World, Settings.GetAudioOutputDeviceId(), FOnCompletedDeviceSwap());
	}
}

void UNGAudioMixEffectsSubsystem::UpdateBusMix(const UNGGameUserSettings& Settings)
{
	if (!UserMix)
	{
		return;
	}

	for (const auto& Entry : Buses)
	{
		const float Volume = Settings.GetVolume(Entry.Key);
		FSoundControlBusMixStage* Stage = UserMix->MixStages.FindByPredicate(
			[Bus = Entry.Value.Get()](const FSoundControlBusMixStage& Candidate) { return Candidate.Bus == Bus; });
		if (!Stage)
		{
			Stage = &UserMix->MixStages.Add_GetRef(
				UAudioModulationStatics::CreateBusMixStage(GetWorld(), Entry.Value, Volume, .05f, .05f));
		}
		Stage->Value.TargetValue = Volume;
	}

	if (!bUserMixActive)
	{
		UAudioModulationStatics::ActivateBusMix(GetWorld(), UserMix);
		bUserMixActive = true;
	}
	else
	{
		UAudioModulationStatics::UpdateMix(GetWorld(), UserMix, UserMix->MixStages, .05f);
	}
}

void UNGAudioMixEffectsSubsystem::ReleaseMixes()
{
	if (UWorld* World = GetWorld())
	{
		if (bUserMixActive && UserMix)
		{
			UAudioModulationStatics::DeactivateBusMix(World, UserMix);
		}
		if (bSoundClassMixActive && SoundClassMix)
		{
			if (FAudioDeviceHandle Device = World->GetAudioDevice())
			{
				Device->PopSoundMixModifier(SoundClassMix);
			}
		}
	}

	bUserMixActive = false;
	bSoundClassMixActive = false;
	bInitializedMixes = false;
	UserMix = nullptr;
	SoundClassMix = nullptr;
	Buses.Reset();
	SoundClasses.Reset();
}
