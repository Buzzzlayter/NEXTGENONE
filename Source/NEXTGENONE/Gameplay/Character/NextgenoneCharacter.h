#pragma once

#include "CoreMinimal.h"
#include "Character/ALSCharacter.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/NGAbilitySystemAvatar.h"
#include "GameplayTagContainer.h"
#include "NextgenoneCharacter.generated.h"

class UNGCombatComponent;
class UNGEquipmentComponent;
class UNGInteractionComponent;
class UNGAimComponent;
class UNGHitReactionComponent;
class UNGCharacterInputComponent;
class UNGCharacterStateMachineComponent;
class UNGCharacterVitalsDefinition;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNGHealthChanged, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNGShieldChanged, float, Shield, float, MaxShield);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGDeathChanged, bool, bDead);

UCLASS(PrioritizeCategories = ("Gameplay"))
class NEXTGENONE_API ANextgenoneCharacter : public AALSCharacter, public IAbilitySystemInterface, public INGAbilitySystemAvatar
{
	GENERATED_BODY()
public:
	ANextgenoneCharacter(const FObjectInitializer& Init);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGEquipmentComponent> Equipment;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGInteractionComponent> Interaction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGAimComponent> Aim;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGHitReactionComponent> HitReaction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGCharacterInputComponent> GameplayInput;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGCharacterStateMachineComponent> CharacterStateMachine;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual bool IsAimCameraActive() const override;
	virtual bool IsPoweredFlightActive() const override;
	virtual void PossessedBy(AController* Controller) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Controller() override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void RagdollStart() override;
	virtual void RagdollEnd() override;
	virtual void ReplicatedRagdollStart() override;
	virtual void ReplicatedRagdollEnd() override;
	virtual void Replicated_PlayMontage_Implementation(UAnimMontage* Montage, float PlayRate) override;
	void ApplyCombatPresentation();
	UFUNCTION(BlueprintPure, Category="Gameplay") bool IsDead() const;
	UFUNCTION(BlueprintPure, Category="Gameplay") float GetHealth() const;
	UFUNCTION(BlueprintPure, Category="Gameplay") float GetMaxHealth() const;
	UFUNCTION(BlueprintPure, Category="Gameplay") float GetShield() const;
	UFUNCTION(BlueprintPure, Category="Gameplay") float GetMaxShield() const;
	UFUNCTION(BlueprintPure, Category="Gameplay") float GetShieldSegmentPercent(int32 SegmentIndex, int32 SegmentCount) const;
	UPROPERTY(BlueprintAssignable) FNGHealthChanged OnHealthChanged;
	UPROPERTY(BlueprintAssignable) FNGShieldChanged OnShieldChanged;
	UPROPERTY(BlueprintAssignable) FNGDeathChanged OnDeathChanged;
	virtual bool CanInitializeGameplay() const override;
	virtual void InitializeLifeAttributes(UAbilitySystemComponent& ASC) const override;
	virtual bool CanReleaseGameplay() const override;
	virtual void TryInitializeGameplay() override;
	virtual void ReleaseGameplayResources() override;
	void UnbindGameplay();
protected:
	UPROPERTY(EditDefaultsOnly, Category="Gameplay|Vitals")
	TObjectPtr<UNGCharacterVitalsDefinition> StartingVitals;

	virtual bool CanAcceptClientLocomotion() const override;
	virtual void OnOverlayStateChanged(EALSOverlayState PreviousState) override;
	virtual void OnGaitChanged(EALSGait PreviousGait) override;
private:
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FDelegateHandle ShieldHandle;
	FDelegateHandle MaxShieldHandle;
	FDelegateHandle RagdollTagHandle;
	void RagdollTagChanged(FGameplayTag Tag, int32 Count);
	void RequestALSAbility(FGameplayTag InputTag);
	void HealthChanged(const FOnAttributeChangeData& Data);
	void ShieldChanged(const FOnAttributeChangeData& Data);
	void StateChanged(FGameplayTag Previous, FGameplayTag Current);
	void StateResourcesFailed();
	void GameplayResourcesReady(AActor* Avatar);
	void RefreshEquipment();
	void UpdateJetpackContext();
};
