#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "NGCombat.generated.h"

class UNGEquipmentComponent;
class ANGWeaponActor;
class UNGEquipmentInstance;
class UAbilitySystemComponent;
class UNGInventoryItemInstance;
class UNGInventoryItemDefinition;
class UNGInventoryManagerComponent;

USTRUCT(BlueprintType)
struct FNGEquipmentState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bEquipped = false;
	UPROPERTY(BlueprintReadOnly) int32 Revision = 0;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UNGWeaponDefinition> Definition;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGEquipmentChanged, FNGEquipmentState, Equipment);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGAmmoChanged, int32, MagazineAmmo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGAimChanged, bool, bAiming);

UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGCombatComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(BlueprintAssignable) FNGEquipmentChanged OnEquipmentChanged;
	UPROPERTY(BlueprintAssignable) FNGAmmoChanged OnAmmoChanged;
	UPROPERTY(BlueprintAssignable, Category="Weapon") FNGAimChanged OnAimChanged;
	UFUNCTION(BlueprintPure) FNGEquipmentState GetEquipment() const;
	UFUNCTION(BlueprintPure) int32 GetMagazineAmmo() const;
	UFUNCTION(BlueprintPure) int32 GetReserveAmmo() const;
	UFUNCTION(BlueprintPure) bool IsAiming() const;
	UFUNCTION(BlueprintPure) UNGInventoryItemInstance* GetEquippedItem() const;
	UNGEquipmentInstance* GetWeaponInstance() const;
	ANGWeaponActor* GetWeaponActor() const;
	static UNGWeaponDefinition* GetWeaponDefinition(const UNGInventoryItemDefinition* ItemDefinition);
	void BindInventory(UNGInventoryManagerComponent* Inventory);
	void BindAbilitySystem(UAbilitySystemComponent* ASC);
	FSimpleMulticastDelegate OnPresentationChanged;
private:
	UNGEquipmentComponent* GetEquipmentComponent() const;
	void EquipmentChanged();
	TWeakObjectPtr<UNGInventoryItemInstance> BoundItem;
	TWeakObjectPtr<UNGInventoryManagerComponent> BoundInventory;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	FDelegateHandle AimingTagHandle;
	UFUNCTION() void OnRep_Equipment();
	UFUNCTION() void OnRep_Ammo();
	UFUNCTION() void OnRep_CurrentItem();
	void AimingTagChanged(FGameplayTag Tag, int32 Count);
	UFUNCTION() void InventoryChanged();
};
