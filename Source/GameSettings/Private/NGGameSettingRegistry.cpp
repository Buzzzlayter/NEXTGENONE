#include "NGGameSettingRegistry.h"

#include "DataSource/GameSettingDataSourceDynamic.h"
#include "EditCondition/WhenCondition.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "GameSettingAction.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "GameSettingValueScalarDynamic.h"
#include "Internationalization/Culture.h"
#include "Internationalization/LocalizedTextSourceTypes.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NGAudioMixEffectsSubsystem.h"
#include "NGGameSettingsConfig.h"
#include "NGGameUserSettings.h"
#include "NGKeyboardSetting.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "NGSettings"

namespace
{
class FSettingCallbacks final : public FGameSettingDataSource
{
public:
	FSettingCallbacks(TFunction<FString()> InRead, TFunction<void(const FString&)> InWrite)
		: Read(MoveTemp(InRead)), Write(MoveTemp(InWrite))
	{
	}

	bool Resolve(ULocalPlayer*) override { return true; }
	FString GetValueAsString(ULocalPlayer*) const override { return Read(); }
	void SetValue(ULocalPlayer*, const FString& Value) override { Write(Value); }
	FString ToString() const override { return TEXT("Settings callbacks"); }

private:
	TFunction<FString()> Read;
	TFunction<void(const FString&)> Write;
};

template<typename T>
T& MakeSetting(UGameSettingCollection& Section, const FName Name, const FText& Label)
{
	T* Setting = NewObject<T>(&Section);
	Setting->SetDevName(Name);
	Setting->SetDisplayName(Label);
	Setting->SetDescriptionRichText(Label);
	return *Setting;
}

template<typename T>
void Bind(T& Setting, UNGGameUserSettings& Draft, const FString& ReadMember, const FString& WriteMember)
{
	Setting.SetDynamicGetter(MakeShared<FGameSettingDataSourceDynamic>(&Draft, ReadMember));
	Setting.SetDynamicSetter(MakeShared<FGameSettingDataSourceDynamic>(&Draft, WriteMember));
}

UGameSettingValueDiscreteDynamic_Bool& AddToggle(UGameSettingCollection& Section, UNGGameUserSettings& Draft,
	FName Name, const FText& Label, bool bDefault, const TCHAR* Getter = nullptr, const TCHAR* Setter = nullptr)
{
	auto& Setting = MakeSetting<UGameSettingValueDiscreteDynamic_Bool>(Section, Name, Label);
	Bind(Setting, Draft, Getter ? FString(Getter) : Name.ToString(), Setter ? FString(Setter) : Name.ToString());
	Setting.SetDefaultValue(bDefault);
	Section.AddSetting(&Setting);
	return Setting;
}

void AddScalar(UGameSettingCollection& Section, UNGGameUserSettings& Draft, FName Name, const FText& Label,
	double Min, double Max, double Step, double Default, const TCHAR* Getter = nullptr, const TCHAR* Setter = nullptr)
{
	auto& Setting = MakeSetting<UGameSettingValueScalarDynamic>(Section, Name, Label);
	Bind(Setting, Draft, Getter ? FString(Getter) : Name.ToString(), Setter ? FString(Setter) : Name.ToString());
	Setting.SetDefaultValue(Default);
	Setting.SetSourceRangeAndStep(TRange<double>(Min, Max), Step);
	Setting.SetMinimumLimit(Min);
	Setting.SetMaximumLimit(Max);
	if (Step >= 1.0)
	{
		Setting.SetDisplayFormat(UGameSettingValueScalarDynamic::SourceAsInteger);
	}
	else
	{
		Setting.SetDisplayFormat(Min == 0.0 && Max == 1.0
			? UGameSettingValueScalarDynamic::ZeroToOnePercent : UGameSettingValueScalarDynamic::RawTwoDecimals);
	}
	Section.AddSetting(&Setting);
}

UGameSettingValueDiscreteDynamic_Number& AddNumbers(UGameSettingCollection& Section, UNGGameUserSettings& Draft,
	FName Name, const FText& Label, int32 Default, TConstArrayView<FText> Options,
	const TCHAR* Getter = nullptr, const TCHAR* Setter = nullptr, int32 FirstValue = 0)
{
	auto& Setting = MakeSetting<UGameSettingValueDiscreteDynamic_Number>(Section, Name, Label);
	Bind(Setting, Draft, Getter ? FString(Getter) : Name.ToString(), Setter ? FString(Setter) : Name.ToString());
	Setting.SetDefaultValue(Default);
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		Setting.AddOption(Index + FirstValue, Options[Index]);
	}
	Section.AddSetting(&Setting);
	return Setting;
}

void BindOptions(UGameSettingValueDiscreteDynamic& Setting, TFunction<FString()> Read, TFunction<void(const FString&)> Write)
{
	const auto Source = MakeShared<FSettingCallbacks>(MoveTemp(Read), MoveTemp(Write));
	Setting.SetDynamicGetter(Source);
	Setting.SetDynamicSetter(Source);
}
}

void UNGGameSettingRegistry::OnInitialize(ULocalPlayer* InLocalPlayer)
{
	UNGGameUserSettings* Live = UNGGameUserSettings::Get();
	if (!Live || !InLocalPlayer)
	{
		return;
	}

	ChangeTracker.StopWatchingRegistry();
	AudioDevice = nullptr;
	Baseline = DuplicateObject<UNGGameUserSettings>(Live, this);
	Draft = DuplicateObject<UNGGameUserSettings>(Live, this);
	Baseline->ScalabilityQuality = Live->ScalabilityQuality;
	Draft->ScalabilityQuality = Live->ScalabilityQuality;
	SelectedCulture = InitialCulture = UKismetInternationalizationLibrary::GetCurrentCulture();

	auto& Gameplay = AddSection(TEXT("GameplayCollection"), LOCTEXT("Gameplay", "Игровые настройки"));
	BuildGameplay(Gameplay);
	auto& Video = AddSection(TEXT("VideoCollection"), LOCTEXT("Video", "Изображение"));
	BuildVideo(Video);
	auto& Audio = AddSection(TEXT("AudioCollection"), LOCTEXT("Audio", "Звук"));
	BuildAudio(Audio);
	auto& Mouse = AddSection(TEXT("MouseAndKeyboardCollection"), LOCTEXT("MouseKeyboard", "Мышь и клавиатура"));
	BuildMouseAndKeyboard(Mouse);
	auto& Gamepad = AddSection(TEXT("GamepadCollection"), LOCTEXT("Gamepad", "Геймпад"));
	BuildGamepad(Gamepad);
	for (UGameSettingCollection* Section : { &Gameplay, &Video, &Audio, &Mouse, &Gamepad })
	{
		RegisterSetting(Section);
	}

	const auto& Hidden = GetDefault<UNGGameSettingsConfig>()->HiddenSettings;
	for (UGameSetting* Setting : RegisteredSettings)
	{
		if (Hidden.Contains(Setting->GetDevName()))
		{
			Setting->AddEditCondition(MakeShared<FWhenCondition>([](const ULocalPlayer*, FGameSettingEditableState& State)
			{
				State.Hide(TEXT("Disabled in Game Settings project configuration"));
			}));
			Setting->RefreshEditableState();
		}
	}
	ChangeTracker.WatchRegistry(this);
}

UGameSettingCollection& UNGGameSettingRegistry::AddSection(FName Name, const FText& Label)
{
	auto* Section = NewObject<UGameSettingCollection>(this);
	Section->SetDevName(Name);
	Section->SetDisplayName(Label);
	Section->Initialize(OwningLocalPlayer);
	return *Section;
}

void UNGGameSettingRegistry::BuildGameplay(UGameSettingCollection& Section)
{
	auto& Language = MakeSetting<UGameSettingValueDiscreteDynamic>(Section, TEXT("Language"), LOCTEXT("Language", "Язык"));
	FTextLocalizationManager& Localization = FTextLocalizationManager::Get();
	TArray<FString> Cultures = Localization.GetLocalizedCultureNames(ELocalizationLoadFlags::Game);
	FString NativeCulture = Localization.GetNativeCultureName(ELocalizedTextSourceCategory::Game);
	if (NativeCulture.IsEmpty())
	{
		NativeCulture = InitialCulture;
	}
	Cultures.AddUnique(NativeCulture);
	Cultures.AddUnique(InitialCulture);
	Cultures.Sort();
	for (const FString& Culture : Cultures)
	{
		const FCulturePtr Info = FInternationalization::Get().GetCulture(Culture);
		Language.AddDynamicOption(Culture, Info.IsValid() ? FText::FromString(Info->GetNativeName()) : FText::FromString(Culture));
	}
	BindOptions(Language, [this] { return SelectedCulture; }, [this](const FString& Value) { SelectedCulture = Value; });
	Language.SetDefaultValueFromString(NativeCulture);
	Section.AddSetting(&Language);
}

void UNGGameSettingRegistry::BuildVideo(UGameSettingCollection& Section)
{
	auto& WindowMode = MakeSetting<UGameSettingValueDiscreteDynamic_Enum>(Section, TEXT("WindowMode"), LOCTEXT("WindowMode", "Режим экрана"));
	Bind(WindowMode, *Draft, TEXT("GetFullscreenMode"), TEXT("SetFullscreenMode"));
	WindowMode.SetDefaultValue(EWindowMode::WindowedFullscreen);
	WindowMode.AddEnumOption(EWindowMode::Fullscreen, LOCTEXT("Fullscreen", "Полноэкранный"));
	WindowMode.AddEnumOption(EWindowMode::WindowedFullscreen, LOCTEXT("Borderless", "Без рамки"));
	WindowMode.AddEnumOption(EWindowMode::Windowed, LOCTEXT("Windowed", "Оконный"));
	Section.AddSetting(&WindowMode);

	auto& Resolution = MakeSetting<UGameSettingValueDiscreteDynamic>(Section, TEXT("Resolution"), LOCTEXT("Resolution", "Разрешение"));
	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	const FIntPoint CurrentResolution = Draft->GetScreenResolution();
	const FIntPoint DesktopResolution = Draft->GetDesktopResolution();
	if (CurrentResolution.X > 0 && CurrentResolution.Y > 0)
	{
		Resolutions.AddUnique(CurrentResolution);
	}
	if (DesktopResolution.X > 0 && DesktopResolution.Y > 0)
	{
		Resolutions.AddUnique(DesktopResolution);
		Resolution.SetDefaultValueFromString(DesktopResolution.ToString());
	}
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
	for (const FIntPoint Size : Resolutions)
	{
		Resolution.AddDynamicOption(Size.ToString(), FText::FromString(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y)));
	}
	BindOptions(Resolution, [this] { return Draft->GetScreenResolution().ToString(); }, [this](const FString& Value)
	{
		FIntPoint Size;
		if (Size.InitFromString(Value))
		{
			Draft->SetScreenResolution(Size);
		}
	});
	Resolution.AddEditCondition(MakeShared<FWhenCondition>([this](const ULocalPlayer*, FGameSettingEditableState& State)
	{
		if (Draft->GetFullscreenMode() == EWindowMode::WindowedFullscreen)
		{
			State.Disable(LOCTEXT("BorderlessResolution", "В режиме без рамки используется разрешение монитора."));
		}
	}));
	Resolution.AddEditDependency(&WindowMode);
	Section.AddSetting(&Resolution);

	auto& Monitor = MakeSetting<UGameSettingValueDiscreteDynamic>(Section, TEXT("MonitorSelector"), LOCTEXT("Monitor", "Монитор"));
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	TArray<FString> DisplayIds;
	for (const FMonitorInfo& Info : Metrics.MonitorInfo)
	{
		DisplayIds.Add(Info.ID);
		Monitor.AddDynamicOption(Info.ID, FText::FromString(Info.Name));
		if (Info.bIsPrimary)
		{
			Monitor.SetDefaultValueFromString(Info.ID);
		}
	}
	BindOptions(Monitor, [this] { return Draft->GetDisplayID(); },
		[this, DisplayIds = MoveTemp(DisplayIds)](const FString& Value)
		{
			const int32 Index = DisplayIds.IndexOfByKey(Value);
			if (Index != INDEX_NONE)
			{
				Draft->SetDisplayProperties(Value, Index);
			}
			else if (Value.IsEmpty())
			{
				Draft->SetDisplayProperties(FString(), INDEX_NONE);
			}
		});
	Section.AddSetting(&Monitor);

	const FText Quality[] = {
		LOCTEXT("Low", "Низкое"), LOCTEXT("Medium", "Среднее"), LOCTEXT("High", "Высокое"),
		LOCTEXT("Epic", "Эпическое"), LOCTEXT("Cinematic", "Кинематографическое")
	};
	auto& Preset = MakeSetting<UGameSettingValueDiscreteDynamic_Number>(Section, TEXT("GraphicsQualityPresets"), LOCTEXT("QualityPreset", "Качество графики"));
	BindOptions(Preset, [this] { return LexToString(Draft->GetOverallScalabilityLevel()); }, [this](const FString& Value)
	{
		const int32 Level = FCString::Atoi(*Value);
		if (Level >= 0)
		{
			Draft->SetOverallScalabilityLevel(Level);
		}
	});
	Preset.SetDefaultValue(2);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Quality); ++Index)
	{
		Preset.AddOption(Index, Quality[Index]);
	}
	Preset.AddOption(-1, LOCTEXT("Custom", "Пользовательское"));
	Section.AddSetting(&Preset);

	if (GetDefault<UNGGameSettingsConfig>()->bEnableHardwareBenchmark)
	{
		auto& Benchmark = MakeSetting<UGameSettingAction>(Section, TEXT("AutoSetQuality"), LOCTEXT("AutoQuality", "Автонастройка графики"));
		Benchmark.SetActionText(LOCTEXT("Benchmark", "Запустить тест"));
		Benchmark.SetDoesActionDirtySettings(true);
		Benchmark.SetCustomAction([this](ULocalPlayer*) { Draft->RunHardwareBenchmark(); });
		Section.AddSetting(&Benchmark);
	}

	AddScalar(Section, *Draft, TEXT("ResolutionScale"), LOCTEXT("ResolutionScale", "Масштаб разрешения"),
		0, 1, .01, 1, TEXT("GetResolutionScaleNormalized"), TEXT("SetResolutionScaleNormalized"));

	struct FQualitySetting
	{
		FName Name;
		FText Label;
		const TCHAR* Getter;
		const TCHAR* Setter;
	};
	const FQualitySetting QualitySettings[] = {
		{ TEXT("GlobalIlluminationQuality"), LOCTEXT("GI", "Глобальное освещение"), TEXT("GetGlobalIlluminationQuality"), TEXT("SetGlobalIlluminationQuality") },
		{ TEXT("Shadows"), LOCTEXT("Shadows", "Тени"), TEXT("GetShadowQuality"), TEXT("SetShadowQuality") },
		{ TEXT("AntiAliasing"), LOCTEXT("AA", "Сглаживание"), TEXT("GetAntiAliasingQuality"), TEXT("SetAntiAliasingQuality") },
		{ TEXT("ViewDistance"), LOCTEXT("ViewDistance", "Дальность прорисовки"), TEXT("GetViewDistanceQuality"), TEXT("SetViewDistanceQuality") },
		{ TEXT("TextureQuality"), LOCTEXT("Textures", "Текстуры"), TEXT("GetTextureQuality"), TEXT("SetTextureQuality") },
		{ TEXT("VisualEffectQuality"), LOCTEXT("Effects", "Визуальные эффекты"), TEXT("GetVisualEffectQuality"), TEXT("SetVisualEffectQuality") },
		{ TEXT("ReflectionQuality"), LOCTEXT("Reflections", "Отражения"), TEXT("GetReflectionQuality"), TEXT("SetReflectionQuality") },
		{ TEXT("PostProcessingQuality"), LOCTEXT("Post", "Постобработка"), TEXT("GetPostProcessingQuality"), TEXT("SetPostProcessingQuality") },
		{ TEXT("FoliageQuality"), LOCTEXT("Foliage", "Растительность"), TEXT("GetFoliageQuality"), TEXT("SetFoliageQuality") },
		{ TEXT("ShadingQuality"), LOCTEXT("Shading", "Затенение"), TEXT("GetShadingQuality"), TEXT("SetShadingQuality") }
	};
	for (const FQualitySetting& Item : QualitySettings)
	{
		AddNumbers(Section, *Draft, Item.Name, Item.Label, 2, Quality, Item.Getter, Item.Setter);
	}
	const FText PostQuality[] = { LOCTEXT("Off", "Выкл."), Quality[1], Quality[2], Quality[3] };
	AddNumbers(Section, *Draft, TEXT("MotionBlurQuality"), LOCTEXT("MotionBlur", "Размытие в движении"), 1, PostQuality);
	AddNumbers(Section, *Draft, TEXT("BloomQuality"), LOCTEXT("Bloom", "Свечение"), 1, PostQuality);
	AddToggle(Section, *Draft, TEXT("VerticalSync"), LOCTEXT("VSync", "Вертикальная синхронизация"), false, TEXT("IsVSyncEnabled"), TEXT("SetVSyncEnabled"));
	AddScalar(Section, *Draft, TEXT("FrameRateLimit"), LOCTEXT("FrameLimit", "Лимит FPS (0 — без ограничения)"),
		0, FMath::Max(360.f, Draft->GetFrameRateLimit()), 1, 0, TEXT("GetFrameRateLimit"), TEXT("SetFrameRateLimit"));
	auto& HDR = AddToggle(Section, *Draft, TEXT("HDRDisplay"), LOCTEXT("HDR", "HDR"), false, TEXT("bUseHDRDisplayOutput"), TEXT("bUseHDRDisplayOutput"));
	HDR.AddEditCondition(MakeShared<FWhenCondition>([this](const ULocalPlayer*, FGameSettingEditableState& State)
	{
		if (!Draft->SupportsHDRDisplayOutput())
		{
			State.Disable(LOCTEXT("HDRUnavailable", "HDR недоступен на этом дисплее."));
		}
	}));
	HDR.RefreshEditableState();

	AddScalar(Section, *Draft, TEXT("Brightness"), LOCTEXT("Gamma", "Яркость"), 1.2, 3.2, .01, 2.2, TEXT("DisplayGamma"), TEXT("DisplayGamma"));
	const FText ColorModes[] = { LOCTEXT("None", "Нет"), LOCTEXT("Deuteranope", "Дейтеранопия"),
		LOCTEXT("Protanope", "Протанопия"), LOCTEXT("Tritanope", "Тританопия") };
	AddNumbers(Section, *Draft, TEXT("ColorBlindMode"), LOCTEXT("ColorMode", "Коррекция цветовосприятия"), 0, ColorModes);
	TArray<FText> Strengths;
	for (int32 Strength = 0; Strength <= 10; ++Strength)
	{
		Strengths.Add(FText::AsNumber(Strength));
	}
	AddNumbers(Section, *Draft, TEXT("ColorBlindStrength"), LOCTEXT("ColorStrength", "Интенсивность коррекции"), 0, Strengths);
}

void UNGGameSettingRegistry::BuildAudio(UGameSettingCollection& Section)
{
	AddScalar(Section, *Draft, TEXT("OverallVolume"), LOCTEXT("MasterVolume", "Общая громкость"), 0, 1, .01, 1);
	AddScalar(Section, *Draft, TEXT("MusicVolume"), LOCTEXT("MusicVolume", "Музыка"), 0, 1, .01, 1);
	AddScalar(Section, *Draft, TEXT("SoundEffectsVolume"), LOCTEXT("FXVolume", "Звуковые эффекты"), 0, 1, .01, 1);
	AddScalar(Section, *Draft, TEXT("UIVolume"), LOCTEXT("UIVolume", "Звуки интерфейса"), 0, 1, .01, 1);
	AddScalar(Section, *Draft, TEXT("AmbientVolume"), LOCTEXT("AmbientVolume", "Окружение"), 0, 1, .01, 1);
	AddScalar(Section, *Draft, TEXT("DialogueVolume"), LOCTEXT("DialogueVolume", "Диалоги"), 0, 1, .01, 1);
	AddToggle(Section, *Draft, TEXT("Subtitles"), LOCTEXT("Subtitles", "Субтитры"), true);
	AddToggle(Section, *Draft, TEXT("BackgroundAudio"), LOCTEXT("BackgroundAudio", "Звук в фоновом режиме"), false);

	AudioDevice = &MakeSetting<UGameSettingValueDiscreteDynamic>(Section, TEXT("AudioOutputDevice"), LOCTEXT("OutputDevice", "Устройство вывода"));
	Bind(*AudioDevice, *Draft, TEXT("AudioOutputDeviceId"), TEXT("AudioOutputDeviceId"));
	AudioDevice->AddDynamicOption(FString(), LOCTEXT("DefaultOutput", "Системное по умолчанию"));
	AudioDevice->SetDefaultValueFromString(FString());
	Section.AddSetting(AudioDevice);
	FOnAudioOutputDevicesObtained OnDevices;
	OnDevices.BindDynamic(this, &ThisClass::OnAudioDevicesEnumerated);
	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(OwningLocalPlayer, OnDevices);
}

void UNGGameSettingRegistry::OnAudioDevicesEnumerated(const TArray<FAudioOutputDeviceInfo>& Devices)
{
	if (!IsValid(AudioDevice))
	{
		return;
	}

	for (const FAudioOutputDeviceInfo& Device : Devices)
	{
		if (!Device.DeviceId.IsEmpty() && !AudioDevice->HasDynamicOption(Device.DeviceId))
		{
			AudioDevice->AddDynamicOption(Device.DeviceId, FText::FromString(Device.Name));
		}
	}
}

void UNGGameSettingRegistry::BuildMouseAndKeyboard(UGameSettingCollection& Section)
{
	AddScalar(Section, *Draft, TEXT("MouseSensitivityYaw"), LOCTEXT("MouseYaw", "Горизонтальная чувствительность мыши"), .01, 10, .01, 1);
	AddScalar(Section, *Draft, TEXT("MouseSensitivityPitch"), LOCTEXT("MousePitch", "Вертикальная чувствительность мыши"), .01, 10, .01, 1);
	AddToggle(Section, *Draft, TEXT("InvertVerticalAxis"), LOCTEXT("InvertY", "Инвертировать вертикальную ось"), false);
	AddToggle(Section, *Draft, TEXT("InvertHorizontalAxis"), LOCTEXT("InvertX", "Инвертировать горизонтальную ось"), false);
	AddScalar(Section, *Draft, TEXT("DashInputDirectionRegistrationDelay"), LOCTEXT("DashDelay", "Задержка регистрации направления рывка (с)"), 0, .5, .01, 0);

	auto* Enhanced = OwningLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	const auto* UserSettings = Enhanced ? Enhanced->GetUserSettings() : nullptr;
	const auto* Profile = UserSettings ? UserSettings->GetActiveKeyProfile() : nullptr;
	if (!Profile)
	{
		return;
	}
	auto& Bindings = MakeSetting<UGameSettingCollection>(Section, TEXT("KeyBindingCollection"), LOCTEXT("KeyBindings", "Клавиши"));
	TMap<FName, FKeyMappingRow> Rows = Profile->GetPlayerMappingRows();
	Rows.KeySort(FNameLexicalLess());
	for (const auto& Row : Rows)
	{
		for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
		{
			const FKey Key = Mapping.GetDefaultKey();
			if (!Key.IsGamepadKey() && !Key.IsTouch() && !Key.IsAnalog())
			{
				auto* Setting = NewObject<UNGKeyboardSetting>(&Bindings);
				Setting->Configure(*Profile, Mapping);
				Bindings.AddSetting(Setting);
			}
		}
	}
	if (Bindings.GetChildSettings().Num() > 0)
	{
		Section.AddSetting(&Bindings);
	}
}

void UNGGameSettingRegistry::BuildGamepad(UGameSettingCollection& Section)
{
	AddToggle(Section, *Draft, TEXT("GamepadVibration"), LOCTEXT("Vibration", "Вибрация"), true);
	AddToggle(Section, *Draft, TEXT("InvertVerticalAxis_Gamepad"), LOCTEXT("InvertY", "Инвертировать вертикальную ось"), false);
	AddToggle(Section, *Draft, TEXT("InvertHorizontalAxis_Gamepad"), LOCTEXT("InvertX", "Инвертировать горизонтальную ось"), false);
	TArray<FText> Presets;
	const int32 Count = GetDefault<UNGGameSettingsConfig>()->GamepadSensitivity.Num();
	if (Count > 0)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Presets.Add(FText::AsNumber(Index + 1));
		}
		const int32 DefaultPreset = FMath::Min(4, Count);
		AddNumbers(Section, *Draft, TEXT("LookSensitivityPreset"), LOCTEXT("GamepadSensitivity", "Чувствительность обзора"), DefaultPreset, Presets, nullptr, nullptr, 1);
		AddNumbers(Section, *Draft, TEXT("LookSensitivityPresetAds"), LOCTEXT("GamepadAdsSensitivity", "Чувствительность прицеливания"), DefaultPreset, Presets, nullptr, nullptr, 1);
	}
	AddScalar(Section, *Draft, TEXT("MoveStickDeadZone"), LOCTEXT("MoveDeadZone", "Мёртвая зона стика движения"), 0, 1, .01, .15);
	AddScalar(Section, *Draft, TEXT("LookStickDeadZone"), LOCTEXT("LookDeadZone", "Мёртвая зона стика обзора"), 0, 1, .01, .15);
}

void UNGGameSettingRegistry::SaveChanges()
{
	UNGGameUserSettings* Live = UNGGameUserSettings::Get();
	if (!Live || !Draft || !HasChanges())
	{
		return;
	}
	ChangeTracker.ApplyChanges();

	const FString PreviousOutputDevice = Live->GetAudioOutputDeviceId();
	CommitMachineSettings(*Live);
	if (SelectedCulture != InitialCulture && UKismetInternationalizationLibrary::SetCurrentCulture(SelectedCulture, true))
	{
		InitialCulture = SelectedCulture;
	}
	if (UWorld* World = OwningLocalPlayer->GetWorld())
	{
		if (APlayerController* Controller = OwningLocalPlayer->UPlayer::GetPlayerController(World))
		{
			Live->ApplyForPlayer(*Controller);
		}
		if (auto* Audio = World->GetSubsystem<UNGAudioMixEffectsSubsystem>())
		{
			Audio->ApplyUserSettings(*Live, PreviousOutputDevice != Live->GetAudioOutputDeviceId());
		}
	}
	if (auto* Enhanced = OwningLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (auto* UserSettings = Enhanced->GetUserSettings())
		{
			UserSettings->ApplySettings();
			UserSettings->SaveSettings();
		}
	}
}

void UNGGameSettingRegistry::CommitMachineSettings(UNGGameUserSettings& LiveSettings)
{
	for (TFieldIterator<FProperty> Field(UNGGameUserSettings::StaticClass()); Field; ++Field)
	{
		if (Field->HasAnyPropertyFlags(CPF_Config) && !Field->Identical_InContainer(Draft, Baseline))
		{
			Field->CopyCompleteValue_InContainer(&LiveSettings, Draft);
			Field->CopyCompleteValue_InContainer(Baseline, Draft);
		}
	}
	if (Draft->ScalabilityQuality != Baseline->ScalabilityQuality)
	{
		LiveSettings.ScalabilityQuality = Draft->ScalabilityQuality;
		Baseline->ScalabilityQuality = Draft->ScalabilityQuality;
	}

	LiveSettings.ApplySettings(false);
	LiveSettings.ConfirmVideoMode();
	LiveSettings.SaveSettings();
}

void UNGGameSettingRegistry::RevertChanges()
{
	if (!Draft)
	{
		return;
	}
	ChangeTracker.RestoreToInitial();
	for (TFieldIterator<FProperty> Field(UNGGameUserSettings::StaticClass()); Field; ++Field)
	{
		if (Field->HasAnyPropertyFlags(CPF_Config))
		{
			Field->CopyCompleteValue_InContainer(Draft, Baseline);
		}
	}
	Draft->ScalabilityQuality = Baseline->ScalabilityQuality;
	SelectedCulture = InitialCulture;

	for (UGameSetting* Setting : RegisteredSettings)
	{
		Setting->RefreshEditableState();
	}
}

void UNGGameSettingRegistry::ResetToDefaults()
{
	for (UGameSetting* Setting : RegisteredSettings)
	{
		const UGameSetting* Ancestor = Setting;
		while (Ancestor && Ancestor->GetEditState().IsVisible() && Ancestor->GetEditState().IsEnabled())
		{
			Ancestor = Ancestor->GetSettingParent();
		}
		if (!Ancestor && Setting->GetEditState().IsResetable())
		{
			if (auto* Value = Cast<UGameSettingValue>(Setting))
			{
				Value->ResetToDefault();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
