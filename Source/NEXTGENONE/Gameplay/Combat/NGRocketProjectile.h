#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "NGRocketProjectile.generated.h"

class APawn;
class UNGPlayerAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraSystem;
class UNiagaraComponent;
class UProjectileMovementComponent;
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

UCLASS(Blueprintable, NotPlaceable, PrioritizeCategories=("Rocket"))
class NEXTGENONE_API ANGRocketProjectile : public AActor
{
	GENERATED_BODY()

public:
	ANGRocketProjectile();
	bool ValidateSettings(FString& Error) const;
	bool Initialize(APawn* Shooter, const FVector& Direction, float Damage, float Range);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void LifeSpanExpired() override;
	virtual void PostNetReceiveLocationAndRotation() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, Category="Rocket|Flight", meta=(ClampMin="100", Units="cm/s")) float LaunchSpeed = 4000.f;
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

	UPROPERTY(VisibleAnywhere, Category="Rocket|Components") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category="Rocket|Components") TObjectPtr<UStaticMeshComponent> RocketMesh;
	UPROPERTY(VisibleAnywhere, Category="Rocket|Components") TObjectPtr<UNiagaraComponent> Trail;

private:
	UFUNCTION() void OnProjectileStopped(const FHitResult& Hit);
	UFUNCTION() void OnRep_Detonation();
	void Detonate(const FVector& Point, const FVector& Normal, const FHitResult* DirectHit);
	void ApplyBlast(const FVector& Origin, const FHitResult* DirectHit);
	void ApplyToCharacter(APawn* Target, const FVector& Origin, const FHitResult* DirectHit);
	UNGPlayerAbilitySystemComponent* GetCurrentSource() const;

	UPROPERTY() TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(ReplicatedUsing=OnRep_Detonation) FNGRocketDetonation Detonation;
	TWeakObjectPtr<UNGPlayerAbilitySystemComponent> SourceASC;
	uint32 SourceGeneration = 0;
	float BaseDamage = 0.f;
	float RemainingRange = 0.f;
	FVector LastFlightPosition = FVector::ZeroVector;
};

UCLASS(Blueprintable, PrioritizeCategories=("Rocket"))
class NEXTGENONE_API ANGAirburstRocketProjectile : public ANGRocketProjectile
{
	GENERATED_BODY()

public:
	ANGAirburstRocketProjectile();
};
