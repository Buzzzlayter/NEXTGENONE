// Copyright. NEXTGENONE — игровой паун (ОБВАЛ, VS0).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "NextgenonePawn.generated.h"

class UCameraComponent;
class USphereComponent;
class UFloatingPawnMovement;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

// Игровой паун VS0: летающая камера + воксельный инструмент (карв/опора по трейсу).
// Сейчас простой летающий вариант на Enhanced Input — основа, дотюним позже.
// Input Actions + Mapping Context создаются в рантайме (паун self-contained, ассеты не нужны).
UCLASS()
class NEXTGENONE_API ANextgenonePawn : public APawn
{
	GENERATED_BODY()

public:
	ANextgenonePawn();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Pawn")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Pawn")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Pawn")
	TObjectPtr<UFloatingPawnMovement> Movement;

	// --- Воксельный инструмент ---
	UPROPERTY(EditAnywhere, Category = "Pawn|Tool")
	float ReachCm = 6000.f;

	UPROPERTY(EditAnywhere, Category = "Pawn|Tool", meta = (ClampMin = "1"))
	float ToolRadiusCm = 25.f;

	UPROPERTY(EditAnywhere, Category = "Pawn|Tool")
	float ToolRadiusMinCm = 5.f;

	UPROPERTY(EditAnywhere, Category = "Pawn|Tool")
	float ToolRadiusMaxCm = 100.f;

	// Материал для ПКМ (наращивание/опора).
	UPROPERTY(EditAnywhere, Category = "Pawn|Tool")
	uint8 BuildMaterial = 1;

	UPROPERTY(EditAnywhere, Category = "Pawn|Tool")
	float LookSensitivity = 1.f;

	// Визуализация трейса инструмента (линия + сфера воздействия). По умолчанию выключено.
	UPROPERTY(EditAnywhere, Category = "Pawn|Tool")
	bool bDrawToolDebug = false;

	// Перф-оверлей (шаг 8): кадр-время + счётчики вокселей/дебриса на экране.
	UPROPERTY(EditAnywhere, Category = "Pawn|Debug")
	bool bShowPerfHUD = false;

	// --- Enhanced Input (рантайм-объекты) ---
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> InputMapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FwdAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BackAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LeftAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookYawAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookPitchAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CarveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BuildAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RadiusAction;

private:
	void EnsureInputObjects();          // идемпотентно создаёт IA + IMC
	void FireTool(uint8 NewMaterial);   // трейс из камеры → ApplyDamageSphere

	void OnFwd(const FInputActionValue& V);
	void OnBack(const FInputActionValue& V);
	void OnRight(const FInputActionValue& V);
	void OnLeft(const FInputActionValue& V);
	void OnLookYaw(const FInputActionValue& V);
	void OnLookPitch(const FInputActionValue& V);
	void OnCarve(const FInputActionValue& V);
	void OnBuild(const FInputActionValue& V);
	void OnRadius(const FInputActionValue& V);
};
