// Copyright. NEXTGENONE — игровой паун (ОБВАЛ, VS0).

#include "NextgenonePawn.h"
#include "VoxelStructure.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"

ANextgenonePawn::ANextgenonePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.15f;   // пушим фокус коллизии структурам ~6–7 раз/сек

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(34.f);
	Collision->SetCollisionProfileName(TEXT("NoCollision"));   // летаем сквозь всё; трейс инструмента отдельно
	SetRootComponent(Collision);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Collision);
	Camera->bUsePawnControlRotation = true;

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->MaxSpeed = 1500.f;
	Movement->Acceleration = 8000.f;
	Movement->Deceleration = 8000.f;

	// Поворот — только у камеры (control rotation), тело не крутим → нет двойного yaw.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void ANextgenonePawn::BeginPlay()
{
	Super::BeginPlay();
	EnsureInputObjects();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// Весь ввод (включая ЛКМ/ПКМ) идёт в игру, а не в Slate/редактор.
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;

		if (UEnhancedInputLocalPlayerSubsystem* Sub =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->AddMappingContext(InputMapping, 0);
		}
	}
}

void ANextgenonePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Пушим позицию камеры как «точку фокуса» воксельным структурам — они сами решают,
	// каким чанкам кукать коллизию. Модуль вокселей про паун ничего не знает (зависимость в одну сторону).
	if (!Camera)
	{
		return;
	}

	const FVector Focus = Camera->GetComponentLocation();
	for (TActorIterator<AVoxelStructure> It(GetWorld()); It; ++It)
	{
		It->SetCollisionFocus(Focus);
	}
}

void ANextgenonePawn::EnsureInputObjects()
{
	if (InputMapping)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type) -> UInputAction*
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};

	FwdAction       = MakeAction(TEXT("IA_Fwd"),       EInputActionValueType::Axis1D);
	BackAction      = MakeAction(TEXT("IA_Back"),      EInputActionValueType::Axis1D);
	RightAction     = MakeAction(TEXT("IA_Right"),     EInputActionValueType::Axis1D);
	LeftAction      = MakeAction(TEXT("IA_Left"),      EInputActionValueType::Axis1D);
	LookYawAction   = MakeAction(TEXT("IA_LookYaw"),   EInputActionValueType::Axis1D);
	LookPitchAction = MakeAction(TEXT("IA_LookPitch"), EInputActionValueType::Axis1D);
	CarveAction     = MakeAction(TEXT("IA_Carve"),     EInputActionValueType::Boolean);
	BuildAction     = MakeAction(TEXT("IA_Build"),     EInputActionValueType::Boolean);
	RadiusAction    = MakeAction(TEXT("IA_Radius"),    EInputActionValueType::Axis1D);

	InputMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Nextgenone"));
	InputMapping->MapKey(FwdAction,       EKeys::W);
	InputMapping->MapKey(BackAction,      EKeys::S);
	InputMapping->MapKey(RightAction,     EKeys::D);
	InputMapping->MapKey(LeftAction,      EKeys::A);
	InputMapping->MapKey(LookYawAction,   EKeys::MouseX);
	InputMapping->MapKey(LookPitchAction, EKeys::MouseY);
	InputMapping->MapKey(CarveAction,     EKeys::LeftMouseButton);
	InputMapping->MapKey(BuildAction,     EKeys::RightMouseButton);
	InputMapping->MapKey(RadiusAction,    EKeys::MouseWheelAxis);
}

void ANextgenonePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	EnsureInputObjects();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(FwdAction,       ETriggerEvent::Triggered, this, &ANextgenonePawn::OnFwd);
		EIC->BindAction(BackAction,      ETriggerEvent::Triggered, this, &ANextgenonePawn::OnBack);
		EIC->BindAction(RightAction,     ETriggerEvent::Triggered, this, &ANextgenonePawn::OnRight);
		EIC->BindAction(LeftAction,      ETriggerEvent::Triggered, this, &ANextgenonePawn::OnLeft);
		EIC->BindAction(LookYawAction,   ETriggerEvent::Triggered, this, &ANextgenonePawn::OnLookYaw);
		EIC->BindAction(LookPitchAction, ETriggerEvent::Triggered, this, &ANextgenonePawn::OnLookPitch);
		EIC->BindAction(CarveAction,     ETriggerEvent::Triggered, this, &ANextgenonePawn::OnCarve);
		EIC->BindAction(BuildAction,     ETriggerEvent::Triggered, this, &ANextgenonePawn::OnBuild);
		EIC->BindAction(RadiusAction,    ETriggerEvent::Triggered, this, &ANextgenonePawn::OnRadius);
	}
}

// --- Движение / обзор (направления из control rotation) ---

void ANextgenonePawn::OnFwd(const FInputActionValue& V)
{
	AddMovementInput(GetControlRotation().Vector(), V.Get<float>());
}

void ANextgenonePawn::OnBack(const FInputActionValue& V)
{
	AddMovementInput(GetControlRotation().Vector(), -V.Get<float>());
}

void ANextgenonePawn::OnRight(const FInputActionValue& V)
{
	AddMovementInput(FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y), V.Get<float>());
}

void ANextgenonePawn::OnLeft(const FInputActionValue& V)
{
	AddMovementInput(FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y), -V.Get<float>());
}

void ANextgenonePawn::OnLookYaw(const FInputActionValue& V)
{
	AddControllerYawInput(V.Get<float>() * LookSensitivity);
}

void ANextgenonePawn::OnLookPitch(const FInputActionValue& V)
{
	AddControllerPitchInput(-V.Get<float>() * LookSensitivity);   // не инвертировано
}

// --- Инструмент ---

void ANextgenonePawn::OnCarve(const FInputActionValue& /*V*/)
{
	FireTool(0);   // 0 = убрать воксели
}

void ANextgenonePawn::OnBuild(const FInputActionValue& /*V*/)
{
	FireTool(BuildMaterial);   // >0 = добавить
}

void ANextgenonePawn::OnRadius(const FInputActionValue& V)
{
	ToolRadiusCm = FMath::Clamp(ToolRadiusCm + V.Get<float>() * 5.f, ToolRadiusMinCm, ToolRadiusMaxCm);
}

void ANextgenonePawn::FireTool(uint8 NewMaterial)
{
	if (!Camera || !GetWorld())
	{
		return;
	}

	const FVector Start = Camera->GetComponentLocation();
	const FVector Dir = Camera->GetForwardVector();
	const FVector End = Start + Dir * ReachCm;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;   // у ProcMesh коллизия complex (per-triangle), simple нет

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	// Визуализация трейса (под галочкой): линия (зелёная — попал, красная — мимо).
	const FVector LineEnd = bHit ? Hit.ImpactPoint : End;
	if (bDrawToolDebug)
	{
		DrawDebugLine(GetWorld(), Start, LineEnd, bHit ? FColor::Green : FColor::Red, false, 1.f, 0, 1.f);
	}

	if (bHit)
	{
		if (AVoxelStructure* Vox = Cast<AVoxelStructure>(Hit.GetActor()))
		{
			// Чуть углубим точку внутрь поверхности, чтобы сфера кусала, а не скользила по грани.
			const FVector P = Hit.ImpactPoint + Dir * (ToolRadiusCm * 0.5f);
			if (bDrawToolDebug)
			{
				// Сфера воздействия инструмента в точке удара.
				DrawDebugSphere(GetWorld(), P, ToolRadiusCm, 12, FColor::Cyan, false, 1.f);
			}
			Vox->ApplyDamageSphere(P, ToolRadiusCm, NewMaterial);
		}
	}
}
