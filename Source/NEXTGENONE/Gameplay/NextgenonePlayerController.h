#pragma once
#include "CoreMinimal.h"
#include "Character/ALSPlayerController.h"
#include "GameplayTagContainer.h"
#include "NextgenonePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UNGInventoryManagerComponent;
class UNGInventoryItemInstance;
class ANextgenoneCharacter;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGInventoryUIRequested, UNGInventoryManagerComponent*, Inventory);
UCLASS()
class NEXTGENONE_API ANextgenonePlayerController : public AALSPlayerController
{
	GENERATED_BODY()
public:
	ANextgenonePlayerController();
	UPROPERTY(EditDefaultsOnly, Category="Abilities|Input") TSoftObjectPtr<UInputMappingContext> AbilityMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Abilities|Input") TSoftObjectPtr<UInputAction> DashInput;
	UPROPERTY(EditDefaultsOnly, Category="Abilities|Input") TSoftObjectPtr<UInputAction> MantleInput;
	UPROPERTY(EditDefaultsOnly, Category="Abilities|Input") TSoftObjectPtr<UInputAction> PickupInput;
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void AddYawInput(float Value) override;
	virtual void AddPitchInput(float Value) override;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bSettingsUIOpen=false;
	void NGFireStopForUI() { PrimaryReleased(); AimReleased(); }
	virtual void OnPossess(APawn* ControlledPawn) override;
	virtual void OnRep_Pawn() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(Exec) void NGEquip();
	UFUNCTION(Exec) void NGUnequip();
	UFUNCTION(Exec) void NGFire();
	UFUNCTION(Exec) void NGPistol();
	UFUNCTION(Exec) void NGRifle();
	UFUNCTION(Exec) void NGShotgun();
	UFUNCTION(Exec) void NGReload();
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory|UI") void OnInventoryRequested(UNGInventoryManagerComponent* Inventory);
	UPROPERTY(BlueprintAssignable, Category="Inventory|UI") FNGInventoryUIRequested InventoryUIRequested;
	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI") bool bInventoryUIOpen = false;
	UFUNCTION(BlueprintCallable, Category="Inventory|UI") void ToggleInventoryUI() { RequestInventoryUI(); }
	UFUNCTION(Exec) void NGDash();
	UFUNCTION(Exec) void NGMantle();
	UFUNCTION(Exec) void NGPickupWeapon();
	UFUNCTION(BlueprintCallable, Category="Inventory") void RequestDropItem(UNGInventoryItemInstance* Item,int32 Count=0);
	UFUNCTION(Server, Reliable) void ServerDropItem(ANextgenoneCharacter* ExpectedPawn,UNGInventoryItemInstance* Item,int32 Count);
	// Invoked only by the dedicated test GameMode when -NGCombatSmoke is present.
	UFUNCTION(Client, Reliable) void ClientSmoke(int32 Command);
	UFUNCTION(Server, Reliable) void ServerSmokeResult(int32 Command, bool bPassed);
private:
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> CombatMapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FireAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> EquipAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> UnequipAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DashAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MantleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PickupAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PistolAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RifleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ShotgunAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AimWeaponAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InventoryAction;
	void RequestInventoryUI();
	FTimerHandle AutoFireTimer;
	void PrimaryPressed(); void PrimaryReleased(); void ContinueAutomaticFire();
	void AimPressed(); void AimReleased();
	void DashPressed(); void DashReleased();
	void MantlePressed(); void MantleReleased();
	void PickupPressed(); void PickupReleased();
	void RouteAbilityInput(FGameplayTag Tag, bool bPressed);
	void UpdateGameplayPawn();
	void RemoveCombatMapping();
};
