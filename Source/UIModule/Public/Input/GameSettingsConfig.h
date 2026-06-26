// Exclusive publisher - Forward Gateway, LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "GameSettingsConfig.generated.h"

class UGamepadCursorWidget;

UENUM(BlueprintType)
enum class EGamepadAccelerationType : uint8
{
	Cubic,
	Linear,
	Curve
};

UCLASS(Config = InputGameSettings, DefaultConfig, meta = (DisplayName = "InputGameSettings"))
class UIMODULE_API UGameSettingsConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGameSettingsConfig();

	static const UGameSettingsConfig* Get();

	UPROPERTY(Config, EditAnywhere, Category = Benchmark)
	bool bShouldRunAutoBenchmarkAtStartup = false;

	UPROPERTY(Config, EditAnywhere, Category = InputSettings)
	FGameplayTag DefaultMappableKeyProfile = FGameplayTag::RequestGameplayTag("InputUserSettings.Profiles.Default");

	UPROPERTY(Config, EditAnywhere, Category = CloudSettings)
	FString CloudSettingsSlotName = TEXT("CloudGameSettings");

	const FRichCurve* GetGamepadCursorAccelerationCurve() const;
	float GetGamepadCursorAccelerationMultiplier() const;
	EGamepadAccelerationType GetGamepadAccelerationType() const;

	float GetMaxGamepadCursorSpeed() const;
	float GetMaxGamepadCursorSpeedWhenHovered() const;
	float GetGamepadCursorDeadZone() const;
	float GetGamepadCursorSize() const;
	const FSoftClassPath& GetGamepadCursorWidgetClass() const;
	float GetGamepadCursorReleaseThreshold() const;
	FVector2D GetGamepadCursorSizeVector() const;
	float GetGamepadCursorRadius() const;
	float GetGamepadScrollDeadZone() const;
	float GetGamepadScrollMultiplier() const;

	float GetMouseMoveThreshold() const;
	float GetGamepadAxisThreshold() const;

protected:
	virtual FName GetCategoryName() const override;

private:
	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Acceleration", meta = (XAxisName = "Strength", YAxisName = "Acceleration"))
	FRuntimeFloatCurve GamepadCursorAccelerationCurve;

	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Acceleration", meta = (ClampMin = "1.0"))
	float GamepadCursorAccelerationMultiplier = 9000.0f;

	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Acceleration")
	EGamepadAccelerationType GamepadAccelerationType = EGamepadAccelerationType::Curve;

	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Speed", meta = (ClampMin = "1.0"))
	float MaxGamepadCursorSpeed = 1300.0f;

	/** The max speed of the Gamepad Cursor when hovering over a widget that is interactable */
	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Speed", meta = (ClampMin = "1.0"))
	float MaxGamepadCursorSpeedWhenHovered = 650.0f;

	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Scroll", meta = (ClampMin = "0.1"))
	float GamepadScrollDeadZone = 0.2f;

	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Scroll", meta = (ClampMin = "0.1"))
	float GamepadScrollMultiplier = 2.5f;

	/** Deadzone value for input from the Gamepad stick */
	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Cursor", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float GamepadCursorDeadZone = 0.15f;

	/** The size (on screen) of the Gamepad cursor */
	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Cursor", meta = (ClampMin = "0.1"))
	float GamepadCursorSize = 90.0f;

	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Cursor", meta = (MetaClass = "/Script/UIModule.GamepadCursorWidget"))
	FSoftClassPath GamepadCursorWidget;

	UPROPERTY(config, EditAnywhere, Category = "GamepadCursor | Cursor", meta = (ClampMin = "0.1"))
	float GamepadCursorReleaseThreshold = 10.f;

	/** The threshold at which mouse detection */
	UPROPERTY(config, EditAnywhere, Category = InputDetector, meta = (ClampMin = "0.1"))
	float MouseMoveThreshold = 1.f;

	/** The threshold at which gamepad detection */
	UPROPERTY(config, EditAnywhere, Category = InputDetector, meta = (ClampMin = "0.1"))
	float GamepadAxisThreshold = 0.2f;
};
