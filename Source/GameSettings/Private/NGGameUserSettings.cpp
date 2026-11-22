#include "NGGameUserSettings.h"

#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "NGGameSettingsConfig.h"
#include "Rendering/SlateRenderer.h"

namespace
{
FVector2D ApplyDeadZone(const FVector2D Value, const float Threshold)
{
	const double Magnitude = Value.Size();
	const double DeadZone = FMath::Clamp(static_cast<double>(Threshold), 0.0, 1.0);
	if (Magnitude <= DeadZone || DeadZone >= 1.0)
	{
		return FVector2D::ZeroVector;
	}
	return Value / Magnitude * FMath::GetMappedRangeValueClamped(
		FVector2D(DeadZone, 1.0), FVector2D(0.0, 1.0), Magnitude);
}
}

UNGGameUserSettings* UNGGameUserSettings::Get()
{
	return GEngine ? Cast<UNGGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UNGGameUserSettings::LoadSettings(bool bForceReload)
{
	if (bForceReload)
	{
		LoadConfigIni(true);
	}
	if (GConfig && !GConfig->DoesSectionExist(*GetClass()->GetPathName(), GGameUserSettingsIni))
	{
		LoadConfig(UGameUserSettings::StaticClass(), *GGameUserSettingsIni);
	}
	Super::LoadSettings(bForceReload);
	if (bImportedInputPreferences || !GConfig)
	{
		return;
	}

	const TCHAR* Section = TEXT("/Script/NEXTGENONE.NGInputPreferences");
	if (GConfig->GetFloat(Section, TEXT("LookSensitivity"), MouseSensitivityYaw, GGameUserSettingsIni))
	{
		MouseSensitivityPitch = MouseSensitivityYaw;
		const auto& Presets = GetDefault<UNGGameSettingsConfig>()->GamepadSensitivity;
		float ClosestDistance = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Presets.Num(); ++Index)
		{
			const float Distance = FMath::Abs(Presets[Index] - MouseSensitivityYaw);
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				LookSensitivityPreset = LookSensitivityPresetAds = Index + 1;
			}
		}
	}
	if (GConfig->GetBool(Section, TEXT("bInvertY"), InvertVerticalAxis, GGameUserSettingsIni))
	{
		InvertVerticalAxis_Gamepad = InvertVerticalAxis;
	}
	GConfig->GetFloat(Section, TEXT("MasterVolume"), OverallVolume, GGameUserSettingsIni);
	bImportedInputPreferences = true;
}

FVector2D UNGGameUserSettings::ScaleLook(FVector2D Look, bool bGamepad, bool bAiming) const
{
	if (bGamepad)
	{
		const TArray<float>& Multipliers = GetDefault<UNGGameSettingsConfig>()->GamepadSensitivity;
		const int32 PresetIndex = (bAiming ? LookSensitivityPresetAds : LookSensitivityPreset) - 1;
		const float Multiplier = Multipliers.IsValidIndex(PresetIndex) ? Multipliers[PresetIndex] : 1.f;
		Look = ApplyDeadZone(Look, LookStickDeadZone) * Multiplier;
		return Look * FVector2D(InvertHorizontalAxis_Gamepad ? -1 : 1, InvertVerticalAxis_Gamepad ? -1 : 1);
	}

	return Look * FVector2D(
		MouseSensitivityYaw * (InvertHorizontalAxis ? -1 : 1),
		MouseSensitivityPitch * (InvertVerticalAxis ? -1 : 1));
}

FVector2D UNGGameUserSettings::FilterMove(FVector2D Move) const
{
	return ApplyDeadZone(Move, MoveStickDeadZone);
}

void UNGGameUserSettings::RunStartupBenchmark()
{
	const auto* Config = GetDefault<UNGGameSettingsConfig>();
	if (FApp::CanEverRender() && Config->bEnableHardwareBenchmark && Config->bRunHardwareBenchmarkOnFirstLaunch
		&& GetLastCPUBenchmarkResult() < 0.f)
	{
		RunHardwareBenchmark();
		ApplyHardwareBenchmarkResults();
	}
}

void UNGGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	if (!FApp::CanEverRender() || !GEngine)
	{
		return;
	}

	GEngine->DisplayGamma = FMath::Clamp(DisplayGamma, 1.2f, 3.2f);
	if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
	{
		Variable->Set(FMath::Clamp(MotionBlurQuality, 0, 3), ECVF_SetByGameOverride);
	}
	if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality")))
	{
		Variable->Set(FMath::Clamp(BloomQuality, 0, 3), ECVF_SetByGameOverride);
	}
	FApp::SetUnfocusedVolumeMultiplier(BackgroundAudio ? 1.f : 0.f);
	UGameplayStatics::SetSubtitlesEnabled(Subtitles);
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		FSlateApplication::Get().GetRenderer()->SetColorVisionDeficiencyType(
			static_cast<EColorVisionDeficiency>(FMath::Clamp(ColorBlindMode, 0, 3)),
			FMath::Clamp(ColorBlindStrength, 0, 10), true, false);
	}
}

void UNGGameUserSettings::ApplyForPlayer(APlayerController& Controller)
{
	Controller.bForceFeedbackEnabled = GamepadVibration;
}

float UNGGameUserSettings::GetVolume(ENGVolumeChannel Channel) const
{
	float Volume = 1.f;
	switch (Channel)
	{
	case ENGVolumeChannel::Overall: Volume = OverallVolume; break;
	case ENGVolumeChannel::Music: Volume = MusicVolume; break;
	case ENGVolumeChannel::Effects: Volume = SoundEffectsVolume; break;
	case ENGVolumeChannel::UI: Volume = UIVolume; break;
	case ENGVolumeChannel::Ambient: Volume = AmbientVolume; break;
	case ENGVolumeChannel::Dialogue: Volume = DialogueVolume; break;
	}
	return FMath::IsFinite(Volume) ? FMath::Clamp(Volume, 0.f, 1.f) : 1.f;
}
