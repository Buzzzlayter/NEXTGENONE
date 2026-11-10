#pragma once

#include "Character/ALSCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementReplication.h"
#include "Gameplay/Equipment/NGJetpackDefinition.h"
#include "NGCharacterMovementComponent.generated.h"

class UAbilitySystemComponent;
class UNGEquipmentInstance;
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

	bool CanStartJetpack() const;
	void SetJetpackRequested(bool bRequested)
	{
		bJetpackRequested = bRequested;
		if (!bRequested) JetpackThrustAlpha = JetpackMoveThrustAlpha = 0.f;
	}
	bool IsJetpackRequested() const { return bJetpackRequested; }
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
	friend class FNGJetpackSavedMove;
    FNGJetpackMoveDataContainer MoveDataContainer;
    FNGJetpackMovementSettings MoveSettings;
    TWeakObjectPtr<UNGEquipmentInstance> ReplaySource;
    int32 MoveGeneration = 0;
    bool bReplaySettings = false;
    bool bReadingNetworkMove = false;
    UNGEquipmentInstance* GetJetpackSource() const;
	bool bJetpackRequested = false;
	float JetpackThrustAlpha = 0.f;
	float JetpackMoveThrustAlpha = 0.f;
	UAbilitySystemComponent* GetJetpackASC() const;
	bool IsJetpackThrustActive() const;
};
