#pragma once

#include "CoreMinimal.h"
#include "Character/ALSCharacter.h"
#include "AbilitySystemInterface.h"
#include "NextgenoneCharacter.generated.h"

class ANextgenonePlayerState;
class UNGCombatComponent;
class UNGMantleAdapterComponent;
class UNGInteractionComponent;
class UNGAimComponent;
class UNGHitReactionComponent;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNGHealthChanged, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGDeathChanged, bool, bDead);

UCLASS()
class NEXTGENONE_API ANextgenoneCharacter : public AALSCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ANextgenoneCharacter(const FObjectInitializer& Init);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGMantleAdapterComponent> MantleAdapter;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGInteractionComponent> Interaction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGAimComponent> Aim;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGHitReactionComponent> HitReaction;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* Controller) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Controller() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void JumpAction_Implementation(bool bValue) override;
	virtual void AimAction_Implementation(bool bValue) override;
	virtual void Tick(float DeltaSeconds) override;
	void ApplyCombatPresentation();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UFUNCTION(BlueprintPure, Category="Gameplay") bool IsDead() const { return bDead; }
	UFUNCTION(BlueprintPure, Category="Gameplay") float GetHealth() const;
	UFUNCTION(BlueprintPure, Category="Gameplay") float GetMaxHealth() const;
	UPROPERTY(BlueprintAssignable) FNGHealthChanged OnHealthChanged;
	UPROPERTY(BlueprintAssignable) FNGDeathChanged OnDeathChanged;
	void TryInitializeGameplay();
	void UnbindGameplay();
protected:
	UPROPERTY(ReplicatedUsing=OnRep_Dead, BlueprintReadOnly, Category="Gameplay") bool bDead = false;
	UFUNCTION() void OnRep_Dead();
private:
	bool bJumpInputHeld = false;
	bool bAimInputHeld = false;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	void HealthChanged(const FOnAttributeChangeData& Data);
};
