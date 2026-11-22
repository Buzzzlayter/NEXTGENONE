#include "NGKeyboardSetting.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputSettings.h"

void UNGKeyboardSetting::Configure(const UEnhancedPlayerMappableKeyProfile& Profile, const FPlayerKeyMapping& Mapping)
{
	MappingArgs.MappingName = Mapping.GetMappingName();
	MappingArgs.Slot = Mapping.GetSlot();
	MappingArgs.ProfileIdString = Profile.GetProfileIdString();
	MappingArgs.HardwareDeviceId = Mapping.GetHardwareDeviceId().HardwareDeviceIdentifier;
	MappingArgs.bCreateMatchingSlotIfNeeded = false;
	MappingArgs.bDeferOnSettingsChangedBroadcast = true;
	SelectedKey = Mapping.GetCurrentKey();
	DefaultKey = Mapping.GetDefaultKey();

	const FText MappingLabel = Mapping.GetDisplayName().IsEmpty() ? FText::FromName(MappingArgs.MappingName) : Mapping.GetDisplayName();
	SetDevName(FName(FString::Printf(TEXT("Key_%s_%d_%s"), *MappingArgs.MappingName.ToString(),
		static_cast<int32>(MappingArgs.Slot), *MappingArgs.HardwareDeviceId.ToString())));
	SetDisplayName(MappingArgs.Slot == EPlayerMappableKeySlot::First ? MappingLabel : FText::Format(
		NSLOCTEXT("NGSettings", "KeySlot", "{0} ({1})"), MappingLabel, static_cast<int32>(MappingArgs.Slot) + 1));
	SetDescriptionRichText(NSLOCTEXT("NGSettings", "RebindHint", "Нажмите клавишу или кнопку мыши."));
}

void UNGKeyboardSetting::SetKey(FKey Key)
{
	if (Key.IsValid() && !Key.IsGamepadKey() && !Key.IsTouch() && !Key.IsAxis2D() && !Key.IsAxis3D() && Key != SelectedKey)
	{
		SelectedKey = Key;
		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

void UNGKeyboardSetting::StoreInitial()
{
	InitialKey = SelectedKey;
}

void UNGKeyboardSetting::RestoreToInitial()
{
	SelectedKey = InitialKey;
	NotifySettingChanged(EGameSettingChangeReason::RestoreToInitial);
}

void UNGKeyboardSetting::ResetToDefault()
{
	SelectedKey = DefaultKey;
	NotifySettingChanged(EGameSettingChangeReason::ResetToDefault);
}

void UNGKeyboardSetting::OnApply()
{
	auto* Enhanced = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	auto* UserSettings = Enhanced ? Enhanced->GetUserSettings() : nullptr;
	if (!UserSettings)
	{
		SelectedKey = InitialKey;
		return;
	}

	FGameplayTagContainer Failures;
	FMapPlayerKeyArgs RequestedMapping = MappingArgs;
	RequestedMapping.NewKey = SelectedKey;
	UserSettings->MapPlayerKey(RequestedMapping, Failures);
	if (!Failures.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not remap %s: %s"), *MappingArgs.MappingName.ToString(), *Failures.ToString());
		SelectedKey = InitialKey;
	}
}
