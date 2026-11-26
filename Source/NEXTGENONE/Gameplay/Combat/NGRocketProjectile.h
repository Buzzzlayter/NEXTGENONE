#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "NGRocketProjectile.generated.h"

class APawn;
class UAudioComponent;
class UNGPlayerAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraSystem;
class UNiagaraComponent;
class UProjectileMovementComponent;
class USceneComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

USTRUCT()
struct FNGRocketDetonation
{
	GENERATED_BODY()

	UPROPERTY() bool bOccurred = false;
	UPROPERTY() FVector_NetQuantize Point = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantizeNormal Normal = FVector::UpVector;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FNGRocketDetonation> : TStructOpsTypeTraitsBase2<FNGRocketDetonation>
{
	enum { WithNetSerializer = true };
};

UCLASS(Blueprintable, NotPlaceable, PrioritizeCategories=("Rocket", "Rocket|FX", "Rocket|Flight", "Rocket|Blast", "Rocket|Components"))
class NEXTGENONE_API ANGRocketProjectile : public AActor
{
	GENERATED_BODY()

public:
	ANGRocketProjectile();
	bool ValidateSettings(FString& Error) const;
	bool Initialize(APawn* Shooter, const FVector& Direction, float Damage, float Range, USoundBase* InLaunchSound);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void LifeSpanExpired() override;
	virtual void PostNetReceiveLocationAndRotation() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, Category="Rocket|Flight", meta=(DisplayName="Top Speed", ClampMin="100", Units="cm/s")) float LaunchSpeed = 2000.f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Flight", meta=(ClampMin="0.1", ClampMax="1")) float InitialSpeedFraction = 0.45f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Flight", meta=(ClampMin="0", Units="s")) float BoostTime = 0.24f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Flight", meta=(ClampMin="0.1", Units="s")) float FlightTime = 4.f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Flight") bool bAirburstOnExpiry = false;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Blast", meta=(ClampMin="1", Units="cm")) float BlastRadius = 450.f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Blast", meta=(ClampMin="0", ClampMax="1")) float EdgeDamageFraction = 0.15f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Blast", meta=(ClampMin="0", Units="cm/s")) float CharacterPushSpeed = 650.f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Blast", meta=(ClampMin="0")) float PhysicsImpulse = 45000.f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Blast", meta=(ClampMin="0")) float DestructionStrain = 250000.f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|Blast") TSubclassOf<UGameplayEffect> PayloadEffect;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|FX") TObjectPtr<UNiagaraSystem> ExplosionFX;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|FX", meta=(ClampMin="0.01")) float ExplosionFXScale = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|FX", meta=(ClampMin="0.1", Units="s")) float ExplosionFXMaxLifetime = 5.f;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|FX") TObjectPtr<USoundBase> ExplosionSound;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|FX") TObjectPtr<UNiagaraSystem> BoostFlashFX;
	UPROPERTY(EditDefaultsOnly, Category="Rocket|FX", meta=(ClampMin="0", Units="deg/s")) float RollSpeed = 105.f;

	UPROPERTY(VisibleAnywhere, Category="Rocket|Components") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category="Rocket|Components") TObjectPtr<UStaticMeshComponent> RocketMesh;
	UPROPERTY(VisibleAnywhere, Category="Rocket|Components") TObjectPtr<UNiagaraComponent> Trail;

private:
	UFUNCTION() void OnProjectileStopped(const FHitResult& Hit);
	UFUNCTION() void OnRep_LaunchSound();
	UFUNCTION() void OnRep_Detonation();
	void UpdateFlightPresentation(float FlightAge);
	void SetTrailThrust(float Thrust);
	void Detonate(const FVector& Point, const FVector& Normal, const FHitResult* DirectHit);
	void ApplyBlast(const FVector& Origin, const FHitResult* DirectHit);
	void ApplyToCharacter(APawn* Target, const FVector& Origin, const FHitResult* DirectHit);
	UNGPlayerAbilitySystemComponent* GetCurrentSource() const;

	UPROPERTY() TObjectPtr<USceneComponent> FlightVisualRoot;
	UPROPERTY() TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(Replicated) double LaunchServerTime = 0.0;
	UPROPERTY(ReplicatedUsing=OnRep_LaunchSound) TObjectPtr<USoundBase> LaunchSound;
	UPROPERTY(ReplicatedUsing=OnRep_Detonation) FNGRocketDetonation Detonation;
	TWeakObjectPtr<UAudioComponent> LaunchAudio;
	TWeakObjectPtr<UNGPlayerAbilitySystemComponent> SourceASC;
	uint32 SourceGeneration = 0;
	float BaseDamage = 0.f;
	float RemainingRange = 0.f;
	FVector LastFlightPosition = FVector::ZeroVector;
	FQuat InitialMeshRotation = FQuat::Identity;
	FVector BaseTrailScale = FVector::OneVector;
	bool bBoostVisualsStarted = false;
};

UCLASS(Blueprintable)
class NEXTGENONE_API ANGAirburstRocketProjectile : public ANGRocketProjectile
{
	GENERATED_BODY()

public:
	ANGAirburstRocketProjectile();
};
