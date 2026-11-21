#include "Gameplay/Character/NGCharacterMovementComponent.h"

#include "Gameplay/Player/NGPlayerContext.h"
#include "GameplayTags/NGGameplayTags.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"

class FNGJetpackSavedMove final : public UALSCharacterMovementComponent::FSavedMove_My
{
	using Super = UALSCharacterMovementComponent::FSavedMove_My;
public:
	virtual void Clear() override
	{
		Super::Clear();
		SavedState = {};
	}
	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Flags = Super::GetCompressedFlags();
		if (SavedState.bRequested) Flags |= FLAG_Custom_1;
		return Flags;
	}
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
		const auto* Movement = Cast<UNGCharacterMovementComponent>(Character->GetCharacterMovement());
		SavedState = Movement ? Movement->CaptureJetpackMove() : FNGJetpackMoveState{};
	}
	virtual void PrepMoveFor(ACharacter* Character) override
	{
		Super::PrepMoveFor(Character);
		if (auto* Movement = Cast<UNGCharacterMovementComponent>(Character->GetCharacterMovement()))
		{
			Movement->RestoreJetpackMove(SavedState);
		}
	}
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
	{
		const auto* Other = static_cast<const FNGJetpackSavedMove*>(NewMove.Get());
		// Do not merge the ramp: otherwise the server integrates a different thrust interval.
		return Other && SavedState.Generation == Other->SavedState.Generation && SavedState.bRequested == Other->SavedState.bRequested
			&& SavedState.ThrustAlpha == Other->SavedState.ThrustAlpha
			&& ((!SavedState.bRequested && SavedState.ThrustAlpha == 0.f) || SavedState.ThrustAlpha == 1.f)
			&& Super::CanCombineWith(NewMove, Character, MaxDelta);
	}
	int32 GetInstallationGeneration() const { return SavedState.Generation; }
private:
	FNGJetpackMoveState SavedState;
};

void FNGJetpackNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	FCharacterNetworkMoveData::ClientFillNetworkMoveData(ClientMove, MoveType);
	InstallationGeneration = static_cast<const FNGJetpackSavedMove&>(ClientMove).GetInstallationGeneration();
}
bool FNGJetpackNetworkMoveData::Serialize(UCharacterMovementComponent& Movement, FArchive& Ar, UPackageMap* Map, ENetworkMoveType MoveType)
{
	const bool Result = FCharacterNetworkMoveData::Serialize(Movement, Ar, Map, MoveType);
	Ar << InstallationGeneration;
	return Result && !Ar.IsError();
}

class FNGClientPredictionData final : public UALSCharacterMovementComponent::FNetworkPredictionData_Client_My
{
public:
	explicit FNGClientPredictionData(const UCharacterMovementComponent& Movement)
		: FNetworkPredictionData_Client_My(Movement) {}
	virtual FSavedMovePtr AllocateNewMove() override { return MakeShared<FNGJetpackSavedMove>(); }
};

UNGCharacterMovementComponent::UNGCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetworkMoveDataContainer(MoveDataContainer);
}

void UNGCharacterMovementComponent::SetJetpackContext(const FNGJetpackMovementSettings& Settings, int32 Generation)
{
	if (ContextGeneration != Generation) SetJetpackRequested(false);
	ContextSettings = Settings;
	ContextGeneration = Generation;
}

FNGJetpackMoveState UNGCharacterMovementComponent::CaptureJetpackMove() const
{
	return {ContextSettings, ContextGeneration, JetpackThrustAlpha, bJetpackRequested};
}

void UNGCharacterMovementComponent::RestoreJetpackMove(const FNGJetpackMoveState& State)
{
	const bool bCurrent = ContextGeneration > 0 && State.Generation == ContextGeneration;
	SetJetpackRequested(bCurrent && State.bRequested);
	JetpackThrustAlpha = bCurrent ? State.ThrustAlpha : 0.f;
	MoveSettings = State.Settings;
	MoveGeneration = bCurrent ? State.Generation : 0;
	bReplaySettings = true;
}

void UNGCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	const auto* Data = static_cast<const FNGJetpackNetworkMoveData*>(GetCurrentNetworkMoveData());
	TGuardValue<bool> Reading(bReadingNetworkMove, CharacterOwner && CharacterOwner->HasAuthority());
	if (bReadingNetworkMove) MoveGeneration = Data ? Data->InstallationGeneration : 0;
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

UAbilitySystemComponent* UNGCharacterMovementComponent::GetJetpackASC() const
{
	if (!CharacterOwner || CharacterOwner->bIsCrouched
		|| (!IsMovingOnGround() && !IsFalling())
		|| ContextGeneration <= 0) return nullptr;
	// Proxies have no Controller; their replicated GAS tag is used only for movement extrapolation.
	auto* ASC = CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CharacterOwner)
		: NGPlayerContext::GetCurrentASC(CharacterOwner, false);
	if (!ASC || ASC->GetAvatarActor() != CharacterOwner || ASC->HasMatchingGameplayTag(TAG_NGDead)
		|| ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f) return nullptr;
	return ASC;
}

bool UNGCharacterMovementComponent::CanStartJetpack() const { return GetJetpackASC() != nullptr; }

void UNGCharacterMovementComponent::SetFlightControl(UObject* FlightOwner, float RequestedAirControl, float RequestedThrustScale)
{
	FlightControlSource = FlightOwner;
	FlightAirControlOverride = FMath::IsFinite(RequestedAirControl) ? FMath::Clamp(RequestedAirControl, 0.f, 1.f) : 0.f;
	FlightThrustScale = FMath::IsFinite(RequestedThrustScale) ? FMath::Clamp(RequestedThrustScale, .5f, 2.f) : 1.f;
}

void UNGCharacterMovementComponent::ClearFlightControl(const UObject* FlightOwner)
{
	if (FlightControlSource.Get() != FlightOwner) return;
	FlightControlSource.Reset();
	FlightAirControlOverride = 0.f;
	FlightThrustScale = 1.f;
}

bool UNGCharacterMovementComponent::IsJetpackThrustActive() const
{
	const bool bProxy = CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy;
	const auto* ASC = (bJetpackRequested || bProxy) ? GetJetpackASC() : nullptr;
	// A compressed input flag alone never authorizes propulsion on the server.
	return ASC && (bProxy || ContextGeneration == MoveGeneration) && ASC->HasMatchingGameplayTag(TAG_NGJetpackActive);
}

void UNGCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bJetpackRequested = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

FNetworkPredictionData_Client* UNGCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner);
	if (!ClientPredictionData)
	{
		auto* MutableThis = const_cast<UNGCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNGClientPredictionData(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}
	return ClientPredictionData;
}

void UNGCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (!bReplaySettings)
	{
		MoveSettings = ContextSettings;
		if (!bReadingNetworkMove) MoveGeneration = ContextGeneration;
	}
	else if (MoveGeneration != ContextGeneration) { MoveGeneration = 0; SetJetpackRequested(false); }
	bReplaySettings = false;
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	JetpackMoveThrustAlpha = 0.f;
	if (!IsJetpackThrustActive()) { JetpackThrustAlpha = 0.f; return; }
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
	if (MoveSettings.ThrustRampTime > UE_SMALL_NUMBER)
	{
		const float RampStep = FMath::Min(DeltaSeconds, (1.f - JetpackThrustAlpha) * MoveSettings.ThrustRampTime);
		const float PreviousAlpha = JetpackThrustAlpha;
		JetpackThrustAlpha = FMath::Min(1.f, PreviousAlpha + RampStep / MoveSettings.ThrustRampTime);
		// Mean thrust over this move, including the portion after reaching full power.
		JetpackMoveThrustAlpha = ((PreviousAlpha + JetpackThrustAlpha) * .5f * RampStep + DeltaSeconds - RampStep) / DeltaSeconds;
	}
	else JetpackMoveThrustAlpha = JetpackThrustAlpha = 1.f;
	if (IsMovingOnGround() && MoveSettings.MaxRiseSpeed > 0.f
		&& GetJetpackThrustAcceleration() > FMath::Abs(GetGravityZ()))
		SetMovementMode(MOVE_Falling);
}

float UNGCharacterMovementComponent::GetJetpackThrustAcceleration() const
{
	return MoveSettings.Acceleration * JetpackMoveThrustAlpha * (FlightControlSource.IsValid() ? FlightThrustScale : 1.f);
}

FVector UNGCharacterMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	FVector Result = Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);
	if (!IsJetpackThrustActive() || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f) return Result;
	const FVector Up = -GetGravityDirection();
	const float RiseSpeed = static_cast<float>(FVector::DotProduct(Result, Up));
	// Keep gravity and existing momentum; add only thrust up to the configured rise speed.
	const float AddedSpeed = FMath::Clamp(MoveSettings.MaxRiseSpeed - RiseSpeed, 0.f, GetJetpackThrustAcceleration() * DeltaTime);
	return Result + Up * AddedSpeed;
}

FVector UNGCharacterMovementComponent::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	const float JetpackAirControl = IsJetpackThrustActive() ? MoveSettings.AirControl : 0.f;
	const float FlightAirControl = FlightControlSource.IsValid() ? FlightAirControlOverride : 0.f;
	return Super::GetAirControl(DeltaTime, FMath::Max(TickAirControl, FMath::Max(JetpackAirControl, FlightAirControl)), FallAcceleration);
}

float UNGCharacterMovementComponent::GetMaxSpeed() const
{
	return IsFalling() && IsJetpackThrustActive() ? MoveSettings.HorizontalSpeed : Super::GetMaxSpeed();
}

float UNGCharacterMovementComponent::GetMaxAcceleration() const
{
	const bool bAirborne = IsFalling();
	const float BaseAcceleration = bAirborne && IsJetpackThrustActive() ? MoveSettings.HorizontalAcceleration : Super::GetMaxAcceleration();
	constexpr float AssistedFlightAccelerationMultiplier = 1.6f;
	return bAirborne && FlightControlSource.IsValid() ? BaseAcceleration * AssistedFlightAccelerationMultiplier : BaseAcceleration;
}

float UNGCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	return IsFalling() && IsJetpackThrustActive() ? MoveSettings.HorizontalBraking : Super::GetMaxBrakingDeceleration();
}

void UNGCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// PhysFalling calls this on horizontal velocity only. Keep its collision/air-control solver.
	// No lateral friction during thrust: brake by the configured deceleration, without erasing inertia on turns.
	Super::CalcVelocity(DeltaTime, IsFalling() && IsJetpackThrustActive() ? 0.f : Friction, bFluid, BrakingDeceleration);
}
