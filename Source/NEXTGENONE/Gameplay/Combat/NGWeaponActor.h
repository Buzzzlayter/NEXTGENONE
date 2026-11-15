#pragma once

#include "Gameplay/Equipment/NGEquipmentVisualActor.h"
#include "Engine/NetSerialization.h"
#include "NGWeaponActor.generated.h"

class UNGWeaponDefinition;
class UNGInventoryItemInstance;
class UNGInventoryManagerComponent;
class USkeletalMeshComponent;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FNGShotCosmetic, FVector, Start, FVector, End, bool, bHit, int32, ShotId);

/** One installed weapon. Server executes shots; clients render the same actor's presentation. */
UCLASS(Blueprintable)
class NEXTGENONE_API ANGWeaponActor : public ANGEquipmentVisualActor
{
	GENERATED_BODY()
public:
	ANGWeaponActor();
	UFUNCTION(BlueprintPure, Category="Weapon") UNGWeaponDefinition* GetDefinition() const;
	UNGInventoryItemInstance* GetItem() const;
	UFUNCTION(BlueprintPure, Category="Weapon") int32 GetMagazineAmmo() const;
	UFUNCTION(BlueprintPure, Category="Weapon") int32 GetReserveAmmo() const;
	virtual FVector GetShotOrigin() const;
	bool TryFireWithViewOnServer(const FVector& ViewOrigin, const FVector& ViewDirection, int32 EquipmentRevision);
	/** Called only by the equipment-granted ability after its GAS commit. */
	bool FireOnServer();
	bool CompleteReloadOnServer();
	UFUNCTION(BlueprintCallable, Category="Weapon") void RequestFire();
	UFUNCTION(NetMulticast, Unreliable) void MulticastReload(UNGWeaponDefinition* Definition);
	UPROPERTY(BlueprintAssignable, Category="Weapon") FNGShotCosmetic OnShotCosmetic;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USkeletalMeshComponent> WeaponMesh;
	/** Server hook after ammo debit. Return false on failure; the caller refunds the same item. */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category="Weapon", meta=(BlueprintProtected)) bool ExecuteFire(FVector ViewOrigin, FVector ViewDirection);
	virtual bool ExecuteFire_Implementation(FVector ViewOrigin, FVector ViewDirection);
	UFUNCTION(BlueprintNativeEvent, Category="Weapon|Presentation") void PlayFireEffects(UNGWeaponDefinition* Definition, FVector Start, FVector End);
	virtual void PlayFireEffects_Implementation(UNGWeaponDefinition* Definition, FVector Start, FVector End);
	UFUNCTION(BlueprintNativeEvent, Category="Weapon|Presentation") void PlayReloadEffects(UNGWeaponDefinition* Definition);
	virtual void PlayReloadEffects_Implementation(UNGWeaponDefinition* Definition);
private:
	UNGInventoryManagerComponent* GetInventory() const;
	APawn* GetPawn() const;
	bool IsCurrentWeapon() const;
	UFUNCTION(Server, Reliable) void ServerRequestFire(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection, int32 EquipmentRevision);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, int32 ShotId, UNGWeaponDefinition* Definition);
	TOptional<FTransform> RequestedShotView;
	int32 ShotSerial = 0;
	bool bCompletingReload = false;
};
