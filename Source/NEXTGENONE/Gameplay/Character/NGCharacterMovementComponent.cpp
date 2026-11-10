#include "Gameplay/Character/NGCharacterMovementComponent.h"

#include "Gameplay/Player/NGPlayerContext.h"
#include "GameplayTags/NGGameplayTags.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"

class FNGJetpackSavedMove final : public UALSCharacterMovementComponent::FSavedMove_My
{
	friend struct FNGJetpackNetworkMoveData;
	using Super = UALSCharacterMovementComponent::FSavedMove_My;
public:
	virtual void Clear() override
	{
		Super::Clear();
		bSavedJetpackRequested = false;
		SavedThrustAlpha = 0.f; SavedSource.Reset(); SavedGeneration = 0;
	}
	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Flags = Super::GetCompressedFlags();
		if (bSavedJetpackRequested) Flags |= FLAG_Custom_1;
		return Flags;
	}
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
		const auto* Movement = Cast<UNGCharacterMovementComponent>(Character->GetCharacterMovement());
		bSavedJetpackRequested = Movement && Movement->IsJetpackRequested();
		SavedThrustAlpha = Movement ? Movement->JetpackThrustAlpha : 0.f;
        auto* Source = Movement ? Movement->GetJetpackSource() : nullptr;
        SavedSource = Source; SavedGeneration = Source ? Source->GetGeneration() : 0;
        if (const auto* Definition = Source ? Cast<UNGJetpackDefinition>(Source->GetDefinition()) : nullptr) SavedSettings = Definition->Movement;
	}
	virtual void PrepMoveFor(ACharacter* Character) override
	{
		Super::PrepMoveFor(Character);
		if (auto* Movement = Cast<UNGCharacterMovementComponent>(Character->GetCharacterMovement()))
		{
			Movement->SetJetpackRequested(bSavedJetpackRequested);
			Movement->JetpackThrustAlpha = SavedThrustAlpha;
            Movement->MoveSettings = SavedSettings; Movement->MoveGeneration = SavedGeneration;
            Movement->ReplaySource = SavedSource; Movement->bReplaySettings = true;
		}
	}
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
	{
		const auto* Other = static_cast<const FNGJetpackSavedMove*>(NewMove.Get());
		// Do not merge the ramp: otherwise the server integrates a different thrust interval.
		return Other && SavedSource == Other->SavedSource && SavedGeneration == Other->SavedGeneration && bSavedJetpackRequested == Other->bSavedJetpackRequested
			&& SavedThrustAlpha == Other->SavedThrustAlpha
			&& ((!bSavedJetpackRequested && SavedThrustAlpha == 0.f) || SavedThrustAlpha == 1.f)
			&& Super::CanCombineWith(NewMove, Character, MaxDelta);
	}
private:
	bool bSavedJetpackRequested = false;
	float SavedThrustAlpha = 0.f;
    TWeakObjectPtr<UNGEquipmentInstance> SavedSource;
    FNGJetpackMovementSettings SavedSettings;
    int32 SavedGeneration = 0;
};

void FNGJetpackNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
    FCharacterNetworkMoveData::ClientFillNetworkMoveData(ClientMove, MoveType);
    InstallationGeneration = static_cast<const FNGJetpackSavedMove&>(ClientMove).SavedGeneration;
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

UNGEquipmentInstance* UNGCharacterMovementComponent::GetJetpackSource() const
{
    const auto* Equipment = CharacterOwner ? CharacterOwner->FindComponentByClass<UNGEquipmentComponent>() : nullptr;
    auto* Source = Equipment ? Equipment->FindInstance(TAG_NGSlotEquipmentBack) : nullptr;
    return Source && Cast<UNGJetpackDefinition>(Source->GetDefinition())
        && (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy || Source->IsCurrent()) ? Source : nullptr;
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
        || !GetJetpackSource()) return nullptr;
	// Proxies have no Controller; their replicated GAS tag is used only for movement extrapolation.
	auto* ASC = CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CharacterOwner)
		: NGPlayerContext::GetCurrentASC(CharacterOwner, false);
	if (!ASC || ASC->GetAvatarActor() != CharacterOwner || ASC->HasMatchingGameplayTag(TAG_NGDead)
		|| ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f) return nullptr;
	return ASC;
}

bool UNGCharacterMovementComponent::CanStartJetpack() const { return GetJetpackASC() != nullptr; }

bool UNGCharacterMovementComponent::IsJetpackThrustActive() const
{
	const bool bProxy = CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy;
	const auto* ASC = (bJetpackRequested || bProxy) ? GetJetpackASC() : nullptr;
	// A compressed input flag alone never authorizes propulsion on the server.
	const auto* Source = GetJetpackSource();
    return ASC && Source && (bProxy || Source->GetGeneration() == MoveGeneration) && ASC->HasMatchingGameplayTag(TAG_NGJetpackActive);
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
    const auto* Source = GetJetpackSource();
    if (!bReplaySettings)
    {
        if (const auto* Definition = Source ? Cast<UNGJetpackDefinition>(Source->GetDefinition()) : nullptr) MoveSettings = Definition->Movement;
        if (!bReadingNetworkMove) MoveGeneration = Source ? Source->GetGeneration() : 0;
    }
    else if (ReplaySource.Get() != Source) { MoveGeneration = 0; bJetpackRequested = false; }
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
	if (IsMovingOnGround()) SetMovementMode(MOVE_Falling);
}

FVector UNGCharacterMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	FVector Result = Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);
	if (!IsJetpackThrustActive() || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f) return Result;
	const FVector Up = -GetGravityDirection();
	const float RiseSpeed = static_cast<float>(FVector::DotProduct(Result, Up));
	// Keep gravity and existing momentum; add only thrust up to the configured rise speed.
	const float AddedSpeed = FMath::Clamp(MoveSettings.MaxRiseSpeed - RiseSpeed, 0.f, MoveSettings.Acceleration * JetpackMoveThrustAlpha * DeltaTime);
	return Result + Up * AddedSpeed;
}

FVector UNGCharacterMovementComponent::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	return Super::GetAirControl(DeltaTime, IsJetpackThrustActive() ? FMath::Max(TickAirControl, MoveSettings.AirControl) : TickAirControl, FallAcceleration);
}

float UNGCharacterMovementComponent::GetMaxSpeed() const
{
	return IsFalling() && IsJetpackThrustActive() ? MoveSettings.HorizontalSpeed : Super::GetMaxSpeed();
}

float UNGCharacterMovementComponent::GetMaxAcceleration() const
{
	return IsFalling() && IsJetpackThrustActive() ? MoveSettings.HorizontalAcceleration : Super::GetMaxAcceleration();
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
