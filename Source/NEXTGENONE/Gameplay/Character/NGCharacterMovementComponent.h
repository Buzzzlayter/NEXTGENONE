#pragma once

#include "Character/ALSCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementReplication.h"
#include "Gameplay/Character/NGJetpackMovementSettings.h"
#include "NGCharacterMovementComponent.generated.h"

class UAbilitySystemComponent;
struct FNGJetpackMoveState
{
	FNGJetpackMovementSettings Settings;
	int32 Generation = 0;
	float ThrustAlpha = 0.f;
	bool bRequested = false;
};

struct FNGJetpackNetworkMoveData : public FCharacterNetworkMoveData
{
	int32 InstallationGeneration = 0;
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& Movement, FArchive& Ar, UPackageMap* Map, ENetworkMoveType MoveType) override;
};
struct FNGJetpackMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
	FNGJetpackMoveDataContainer() { NewMoveData = &Moves[0]; PendingMoveData = &Moves[1]; OldMoveData = &Moves[2]; }
	FNGJetpackNetworkMoveData Moves[3];
};

/** ALS locomotion plus predicted jetpack intent; GAS authorizes thrust on the current Avatar. */
UCLASS()
class NEXTGENONE_API UNGCharacterMovementComponent : public UALSCharacterMovementComponent
{
	GENERATED_BODY()
public:
	UNGCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditDefaultsOnly, Category = "Character Movement: Walking", meta = (ClampMin = "0.1", ClampMax = "1", DisplayPriority = "1"))
	float GroundResponseScale = 1.f;

	void SetJetpackContext(const FNGJetpackMovementSettings& Settings, int32 Generation);
	FNGJetpackMoveState CaptureJetpackMove() const;
	void RestoreJetpackMove(const FNGJetpackMoveState& State);
	bool CanStartJetpack() const;
	bool IsJetpackThrustActive() const;
	void SetFlightControl(UObject* FlightOwner, float RequestedAirControl, float RequestedThrustScale);
	void ClearFlightControl(const UObject* FlightOwner);
	void SetJetpackRequested(bool bRequested)
	{
		bJetpackRequested = bRequested;
		if (!bRequested) JetpackThrustAlpha = JetpackMoveThrustAlpha = 0.f;
	}
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;
	virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
private:
	FNGJetpackMoveDataContainer MoveDataContainer;
	FNGJetpackMovementSettings MoveSettings;
	FNGJetpackMovementSettings ContextSettings;
	int32 ContextGeneration = 0;
	int32 MoveGeneration = 0;
	bool bReplaySettings = false;
	bool bReadingNetworkMove = false;
	bool bJetpackRequested = false;
	float JetpackThrustAlpha = 0.f;
	float JetpackMoveThrustAlpha = 0.f;
	TWeakObjectPtr<UObject> FlightControlSource;
	float FlightAirControlOverride = 0.f;
	float FlightThrustScale = 1.f;
	UAbilitySystemComponent* GetJetpackASC() const;
	float GetJetpackThrustAcceleration() const;
};
