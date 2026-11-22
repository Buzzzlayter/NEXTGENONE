#pragma once

#include "GameSettingValue.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "NGKeyboardSetting.generated.h"

UCLASS()
class UNGKeyboardSetting : public UGameSettingValue
{
	GENERATED_BODY()

public:
	void Configure(const UEnhancedPlayerMappableKeyProfile& Profile, const FPlayerKeyMapping& Mapping);
	void SetKey(FKey Key);
	FKey GetKey() const { return SelectedKey; }

	void StoreInitial() override;
	void RestoreToInitial() override;
	void ResetToDefault() override;

protected:
	void OnApply() override;

private:
	FMapPlayerKeyArgs MappingArgs;
	FKey InitialKey;
	FKey SelectedKey;
	FKey DefaultKey;
};
