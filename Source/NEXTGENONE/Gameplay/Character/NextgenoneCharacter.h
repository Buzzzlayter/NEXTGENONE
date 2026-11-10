#pragma once

#include "CoreMinimal.h"
#include "Character/ALSCharacter.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "NextgenoneCharacter.generated.h"

class ANextgenonePlayerState;
class UNGCombatComponent;
class UNGEquipmentComponent;
class UNGInteractionComponent;
class UNGAimComponent;
class UNGHitReactionComponent;
class UNGCharacterInputComponent;
class UNGCharacterStateMachineComponent;
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGEquipmentComponent> Equipment;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGInteractionComponent> Interaction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGAimComponent> Aim;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGHitReactionComponent> HitReaction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGCharacterInputComponent> GameplayInput;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay") TObjectPtr<UNGCharacterStateMachineComponent> CharacterStateMachine;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* Controller) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Controller() override;
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
	UPROPERTY(BlueprintAssignable) FNGHealthChanged OnHealthChanged;
	UPROPERTY(BlueprintAssignable) FNGDeathChanged OnDeathChanged;
	void TryInitializeGameplay();
	void UnbindGameplay();
protected:
	virtual bool CanAcceptClientLocomotion() const override;
	virtual void OnOverlayStateChanged(EALSOverlayState PreviousState) override;
	virtual void OnGaitChanged(EALSGait PreviousGait) override;
private:
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FDelegateHandle RagdollTagHandle;
	void RagdollTagChanged(FGameplayTag Tag, int32 Count);
	void RequestALSAbility(FGameplayTag InputTag);
	void HealthChanged(const FOnAttributeChangeData& Data);
	void StateChanged(FGameplayTag Previous, FGameplayTag Current);
	void StateResourcesFailed();
	void EquipmentResourcesReady(AActor* Avatar);
	void RefreshEquipment();
};
