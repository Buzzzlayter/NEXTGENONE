#pragma once

#include "Gameplay/Equipment/NGEquipmentVisualActor.h"
#include "Engine/NetSerialization.h"
#include "NGWeaponActor.generated.h"

class UNGWeaponDefinition;
class UNGInventoryItemInstance;
class USkeletalMeshComponent;
class UNGLaserSightComponent;
class APawn;
struct FCollisionQueryParams;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FNGShotCosmetic, FVector, Start, FVector, End, bool, bHit, int32, ShotId);

/** One installed weapon. Server executes shots; clients render the same actor's presentation. */
UCLASS(Blueprintable)
class NEXTGENONE_API ANGWeaponActor : public ANGEquipmentVisualActor
{
	GENERATED_BODY()
public:
	ANGWeaponActor();
	virtual void BeginPlay() override;
	void UpdateAimPresentation(const FVector& ViewOrigin, const FVector& ViewDirection);
	void SetLaserSightVisible(bool bVisible);
	UFUNCTION(BlueprintPure, Category="Weapon") UNGWeaponDefinition* GetDefinition() const;
	UNGInventoryItemInstance* GetItem() const;
	UFUNCTION(BlueprintPure, Category="Weapon") int32 GetMagazineAmmo() const;
	virtual FVector GetShotOrigin() const;
	/** Called only by the equipment-granted ability after its GAS commit. */
	bool FireOnServer(FVector ViewOrigin, FVector ViewDirection);
	UFUNCTION(NetMulticast, Unreliable) void MulticastReload(UNGWeaponDefinition* Definition);
	UPROPERTY(BlueprintAssignable, Category="Weapon") FNGShotCosmetic OnShotCosmetic;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USkeletalMeshComponent> WeaponMesh;
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Aim") TSubclassOf<UNGLaserSightComponent> LaserSightClass;
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Aim") FName LaserSightSocket = TEXT("Muzzle");
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Aim") FTransform LaserSightOffset = FTransform::Identity;
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Aim") TObjectPtr<UNGLaserSightComponent> LaserSight;
	/** Server hook after ammo debit. Return false on failure; the caller refunds the same item. */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category="Weapon", meta=(BlueprintProtected)) bool ExecuteFire(FVector ViewOrigin, FVector ViewDirection);
	virtual bool ExecuteFire_Implementation(FVector ViewOrigin, FVector ViewDirection);
	UFUNCTION(BlueprintNativeEvent, Category="Weapon|Presentation") void PlayFireEffects(UNGWeaponDefinition* Definition, FVector Start, FVector End);
	virtual void PlayFireEffects_Implementation(UNGWeaponDefinition* Definition, FVector Start, FVector End);
	UFUNCTION(BlueprintNativeEvent, Category="Weapon|Presentation") void PlayReloadEffects(UNGWeaponDefinition* Definition);
	virtual void PlayReloadEffects_Implementation(UNGWeaponDefinition* Definition);
private:
	APawn* GetPawn() const;
	bool HasWeaponContext() const;
	bool ResolveShotPath(const FVector& ViewOrigin, const FVector& ViewDirection, const UNGWeaponDefinition& Definition,
		FVector& OutStart, FVector& OutEnd, FCollisionQueryParams& Params) const;
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, int32 ShotId, UNGWeaponDefinition* Definition);
	int32 ShotSerial = 0;
};
