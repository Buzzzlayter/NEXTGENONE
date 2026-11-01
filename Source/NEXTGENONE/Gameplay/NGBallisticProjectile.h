#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NGBallisticProjectile.generated.h"
class USphereComponent;
class UStaticMeshComponent;
class UAbilitySystemComponent;
class ANextgenoneCharacter;
class UNGCombatComponent;
class UNGWeaponDefinition;
class UNiagaraSystem;
class UNiagaraComponent;

UCLASS()
class NEXTGENONE_API UNGBallisticMovement : public UProjectileMovementComponent
{
	GENERATED_BODY()
public:
	FVector Gravity=FVector(0,0,-980);
	float Drag=0.f;
	virtual FVector ComputeAcceleration(const FVector& InVelocity,float DeltaTime) const override { return Gravity-InVelocity*Drag; }
};

/** Swept, substepped server collision; client simulation is cosmetic only. */
UCLASS(Blueprintable)
class NEXTGENONE_API ANGBallisticProjectile : public AActor
{
	GENERATED_BODY()
public:
	ANGBallisticProjectile();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void Tick(float Delta) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual void PostNetReceiveVelocity(const FVector& Velocity) override;
	void Initialize(ANextgenoneCharacter* Shooter,UNGCombatComponent* Combat,FVector Velocity,FVector Gravity,float Drag,float Damage,float Range,int32 Shot,float ImpactImpulse);
	void SetWeaponEffects(UNGWeaponDefinition* Definition);
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UNGBallisticMovement> Movement;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Ballistics) FVector_NetQuantize InitialVelocity;
	UPROPERTY(ReplicatedUsing=OnRep_Ballistics) FVector_NetQuantize Gravity;
	UPROPERTY(ReplicatedUsing=OnRep_Ballistics) float Drag=0.f;
	UPROPERTY(ReplicatedUsing=OnRep_Impact) bool bImpacted=false;
	UPROPERTY(ReplicatedUsing=OnRep_Tracer) TObjectPtr<UNiagaraSystem> TracerFX;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Tracer;
	UPROPERTY() TObjectPtr<UNGWeaponDefinition> ShotWeapon;
	FVector LastVFXPosition;
	bool bTracerRequested=false,bTracerTrigger=false;
	UFUNCTION() void OnRep_Tracer();
	void StopTracer();
	TWeakObjectPtr<ANextgenoneCharacter> SourcePawn;
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	TWeakObjectPtr<UNGCombatComponent> SourceCombat;
	uint32 SourceLife=0;
	float Damage=0.f,MaxRange=10000.f,Travelled=0.f;
	float BodyImpulse=1600.f;
	FVector PreviousPosition;
	int32 ShotId=0;
	UFUNCTION() void OnStopped(const FHitResult& Hit);
	UFUNCTION() void OnRep_Ballistics();
	UFUNCTION() void OnRep_Impact();
};
