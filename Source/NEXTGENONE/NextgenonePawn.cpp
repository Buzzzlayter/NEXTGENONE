// Copyright. NEXTGENONE — игровой паун.

#include "NextgenonePawn.h"
#include "VoxelPawnMovementComponent.h"
#include "VoxelRuntimeRegistry.h"
#include "VoxelStructure.h"
#include "VoxelWorld.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Field/FieldSystemComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"

ANextgenonePawn::ANextgenonePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

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

	VoxelMovement = CreateDefaultSubobject<UVoxelPawnMovementComponent>(TEXT("VoxelMovement"));
	VoxelMovement->SetUpdatedComponent(Collision);
	VoxelMovement->SetFocusComponent(Camera);

	// Поле Chaos-разрушения. Транзиентные команды (ApplyStrainField/ApplyRadialForce) действуют
	// на все Geometry Collections в радиусе — компоненту достаточно быть в мире (на пауне).
	Field = CreateDefaultSubobject<UFieldSystemComponent>(TEXT("Field"));
	Field->SetupAttachment(Collision);

	// Поворот — только у камеры (control rotation), тело не крутим → нет двойного yaw.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void ANextgenonePawn::BeginPlay()
{
	Super::BeginPlay();
	EnsureInputObjects();
	if (VoxelMovement)
	{
		VoxelMovement->SetUpdatedComponent(Collision);
		VoxelMovement->SetFocusComponent(Camera);
	}

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

	if (!GetWorld())
	{
		return;
	}

	if (bShowPerfHUD && GEngine)
	{
		const float Delta = GetWorld()->GetDeltaSeconds();
		const float FrameMs = 1000.f * Delta;
		const float Fps = (Delta > 0.f) ? (1.f / Delta) : 0.f;
		GEngine->AddOnScreenDebugMessage(101, 0.3f, FColor::Green,
			FString::Printf(TEXT("frame %.1f ms  (%.0f fps)"), FrameMs, Fps));
		GEngine->AddOnScreenDebugMessage(102, 0.3f, FColor::White,
			FString::Printf(TEXT("voxel: %d actors, %d chunks, %d sections | tool r=%.0f"),
				VoxelMovement ? VoxelMovement->GetCachedNumVoxelActors() : 0,
				VoxelMovement ? VoxelMovement->GetCachedNumChunks() : 0,
				VoxelMovement ? VoxelMovement->GetCachedNumSections() : 0,
				ToolRadiusCm));
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
	JumpAction      = MakeAction(TEXT("IA_Jump"),      EInputActionValueType::Boolean);
	ExplodeAction   = MakeAction(TEXT("IA_Explode"),   EInputActionValueType::Boolean);

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
	InputMapping->MapKey(JumpAction,      EKeys::SpaceBar);
	InputMapping->MapKey(ExplodeAction,   EKeys::F);
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
		EIC->BindAction(JumpAction,      ETriggerEvent::Started, this, &ANextgenonePawn::OnJump);
		EIC->BindAction(ExplodeAction,   ETriggerEvent::Triggered, this, &ANextgenonePawn::OnExplode);
	}
}

FVector ANextgenonePawn::GetYawForward() const
{
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	return FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
}

FVector ANextgenonePawn::GetYawRight() const
{
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	return FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
}

// --- Движение / обзор (направления из control rotation) ---

void ANextgenonePawn::OnFwd(const FInputActionValue& V)
{
	AddMovementInput(GetYawForward(), V.Get<float>());
}

void ANextgenonePawn::OnBack(const FInputActionValue& V)
{
	AddMovementInput(GetYawForward(), -V.Get<float>());
}

void ANextgenonePawn::OnRight(const FInputActionValue& V)
{
	AddMovementInput(GetYawRight(), V.Get<float>());
}

void ANextgenonePawn::OnLeft(const FInputActionValue& V)
{
	AddMovementInput(GetYawRight(), -V.Get<float>());
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

void ANextgenonePawn::OnJump(const FInputActionValue& /*V*/)
{
	if (VoxelMovement)
	{
		VoxelMovement->Jump();
	}
}

void ANextgenonePawn::OnExplode(const FInputActionValue& /*V*/)
{
	if (!Field || !Camera || !GetWorld())
	{
		return;
	}

	// Точка взрыва: трейс из камеры; попал → точка удара, мимо → точка на дальности досягаемости.
	const FVector Start = Camera->GetComponentLocation();
	const FVector Dir = Camera->GetForwardVector();
	const FVector End = Start + Dir * ReachCm;

	bool bHitVoxel = false;
	FVector BestVoxelImpact = End;
	float BestVoxelDistSq = TNumericLimits<float>::Max();
	FVoxelRuntimeRegistry::ForEachStructure(GetWorld(), [&](AVoxelStructure& Structure)
	{
		FVector Impact;
		FVector Normal;
		if (Structure.RaycastSolidVoxel(Start, End, Impact, Normal))
		{
			const float DistSq = FVector::DistSquared(Start, Impact);
			if (DistSq < BestVoxelDistSq)
			{
				BestVoxelDistSq = DistSq;
				BestVoxelImpact = Impact;
				bHitVoxel = true;
			}
		}
	});
	FVoxelRuntimeRegistry::ForEachWorld(GetWorld(), [&](AVoxelWorld& VoxelWorld)
	{
		FVector Impact;
		FVector Normal;
		if (VoxelWorld.RaycastSolidVoxel(Start, End, Impact, Normal))
		{
			const float DistSq = FVector::DistSquared(Start, Impact);
			if (DistSq < BestVoxelDistSq)
			{
				BestVoxelDistSq = DistSq;
				BestVoxelImpact = Impact;
				bHitVoxel = true;
			}
		}
	});

	FVector P = bHitVoxel ? BestVoxelImpact : End;
	if (!bHitVoxel)
	{
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			P = Hit.ImpactPoint;
		}
	}

	if (bDrawToolDebug)
	{
		DrawDebugSphere(GetWorld(), P, ExplodeRadiusCm, 16, FColor::Orange, false, 1.5f);
	}

	// 1) Strain рвёт кластеры Geometry Collection в радиусе (должен превышать Damage Threshold).
	Field->ApplyStrainField(true, P, ExplodeRadiusCm, ExplodeStrain, ExplodeClusterLevels);
	// 2) Радиальная сила с затуханием расталкивает отколовшиеся куски от центра взрыва.
	Field->ApplyRadialVectorFalloffForce(true, P, ExplodeRadiusCm, ExplodeForce);
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

	AVoxelStructure* BestVoxel = nullptr;
	AVoxelWorld* BestWorld = nullptr;
	FVector BestImpact = End;
	float BestDistSq = TNumericLimits<float>::Max();
	FVoxelRuntimeRegistry::ForEachStructure(GetWorld(), [&](AVoxelStructure& Structure)
	{
		FVector Impact;
		FVector Normal;
		if (Structure.RaycastSolidVoxel(Start, End, Impact, Normal))
		{
			const float DistSq = FVector::DistSquared(Start, Impact);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestImpact = Impact;
				BestVoxel = &Structure;
			}
		}
	});
	FVoxelRuntimeRegistry::ForEachWorld(GetWorld(), [&](AVoxelWorld& VoxelWorld)
	{
		FVector Impact;
		FVector Normal;
		if (VoxelWorld.RaycastSolidVoxel(Start, End, Impact, Normal))
		{
			const float DistSq = FVector::DistSquared(Start, Impact);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestImpact = Impact;
				BestVoxel = nullptr;
				BestWorld = &VoxelWorld;
			}
		}
	});

	if (!BestVoxel && !BestWorld)
	{
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		Params.bTraceComplex = true;

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			if (AVoxelStructure* HitVoxel = Cast<AVoxelStructure>(Hit.GetActor()))
			{
				BestImpact = Hit.ImpactPoint;
				BestVoxel = HitVoxel;
				BestWorld = nullptr;
			}
		}
	}

	// Визуализация трейса (под галочкой): линия (зелёная — попал, красная — мимо).
	const bool bHitVoxel = BestVoxel || BestWorld;
	const FVector LineEnd = bHitVoxel ? BestImpact : End;
	if (bDrawToolDebug)
	{
		DrawDebugLine(GetWorld(), Start, LineEnd, bHitVoxel ? FColor::Green : FColor::Red, false, 1.f, 0, 1.f);
	}

	if (bHitVoxel)
	{
		// Чуть углубим точку внутрь поверхности, чтобы сфера кусала, а не скользила по грани.
		const FVector P = BestImpact + Dir * (ToolRadiusCm * 0.5f);
		if (bDrawToolDebug)
		{
			// Сфера воздействия инструмента в точке удара.
			DrawDebugSphere(GetWorld(), P, ToolRadiusCm, 12, FColor::Cyan, false, 1.f);
		}
		if (BestVoxel)
		{
			BestVoxel->ApplyDamageSphere(P, ToolRadiusCm, NewMaterial);
		}
		else if (BestWorld)
		{
			BestWorld->ApplyDamageSphere(P, ToolRadiusCm, NewMaterial);
		}
	}
}
