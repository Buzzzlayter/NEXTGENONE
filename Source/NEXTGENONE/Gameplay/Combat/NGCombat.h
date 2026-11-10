#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Library/ALSCharacterEnumLibrary.h"
#include "NGCombat.generated.h"

class UNGEquipmentComponent;
class APawn;
class UNGEquipmentInstance;
class UAbilitySystemComponent;
class UAnimSequence;
class UAnimMontage;
class UGameplayAbility;
class UStaticMesh;
class USkeletalMeshComponent;
class UNGInventoryItemInstance;
class UNGInventoryItemDefinition;
class UNGInventoryManagerComponent;
class ANGBallisticProjectile;
class USoundBase;
class UNiagaraSystem;

UCLASS(BlueprintType, Blueprintable)
class NEXTGENONE_API UNGWeaponDefinition : public UNGEquipmentDefinition
{
	GENERATED_BODY()
public:
	UNGWeaponDefinition();
	virtual bool ValidateDefinition(FString& Error) const override;
	virtual void ResolveResources(TArray<FNGEquipmentAbilityGrant>& OutAbilities, TArray<TSubclassOf<UGameplayEffect>>& OutEffects) const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ammo") TSoftClassPtr<UNGInventoryItemDefinition> AmmoItemClass;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics") TSubclassOf<ANGBallisticProjectile> ProjectileClass;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="100",ClampMax="200000")) float MuzzleSpeed=60000.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="0",ClampMax="10")) float GravityScale=1.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="0",ClampMax="2")) float LinearDrag=0.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="1",ClampMax="16")) int32 PelletsPerShot=1;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="0",ClampMax="20")) float SpreadDegrees=0.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="0",ClampMax="10000")) float BodyImpactImpulse=1600.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Cosmetics") TObjectPtr<USoundBase> FireSound;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Cosmetics|Niagara") TObjectPtr<UNiagaraSystem> MuzzleFX;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Cosmetics|Niagara") TObjectPtr<UNiagaraSystem> ShellEjectFX;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Cosmetics|Niagara") TObjectPtr<UNiagaraSystem> TracerFX;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Cosmetics|Niagara") TObjectPtr<UNiagaraSystem> ImpactFX;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Cosmetics|Niagara") TObjectPtr<UNiagaraSystem> CharacterImpactFX;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Cosmetics|Niagara") TObjectPtr<UStaticMesh> ShellMesh;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Cosmetics|Niagara") FName MuzzleFXSocket=TEXT("Muzzle");
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Cosmetics|Niagara") FName ShellEjectFXSocket=TEXT("ShellEject");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TSoftClassPtr<UNGInventoryItemDefinition> InventoryItemClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) EALSOverlayState Overlay = EALSOverlayState::Default;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bAutomatic = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0")) int32 InitialReserveAmmo = 90;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.1")) float ReloadDuration = 1.4f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimSequence> WeaponFireAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimSequence> WeaponReloadAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimMontage> CharacterFireMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimMontage> CharacterReloadMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon") TSoftClassPtr<UGameplayAbility> FireAbilityClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon") TSoftClassPtr<UGameplayAbility> ReloadAbilityClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon") TSubclassOf<UGameplayAbility> AimAbilityClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) float Damage = 25.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) float Range = 10000.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.05")) float FireInterval = 0.25f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) int32 MagazineCapacity = 12;
};

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FNGShotCosmetic, FVector, Start, FVector, End, bool, bHit, int32, ShotId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGAimChanged, bool, bAiming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNGProjectileImpact,FVector,Point,FVector,Normal,int32,ShotId);

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
	// Weapon-pack adapters subscribe here; cosmetic events never apply damage.
	UPROPERTY(BlueprintAssignable) FNGShotCosmetic OnShotCosmetic;
	UPROPERTY(BlueprintAssignable) FNGProjectileImpact OnProjectileImpact;
	UPROPERTY(BlueprintAssignable, Category="Weapon") FNGAimChanged OnAimChanged;
	UFUNCTION(BlueprintPure) FNGEquipmentState GetEquipment() const;
	UFUNCTION(BlueprintPure) int32 GetMagazineAmmo() const;
	UFUNCTION(BlueprintPure) int32 GetReserveAmmo() const;
	UFUNCTION(BlueprintPure) bool IsAiming() const;
	UFUNCTION(BlueprintPure) UNGInventoryItemInstance* GetEquippedItem() const;
	UNGEquipmentInstance* GetWeaponInstance() const;
	UFUNCTION(BlueprintCallable) void RequestReload();
	static UNGWeaponDefinition* GetWeaponDefinition(const UNGInventoryItemDefinition* ItemDefinition);
	bool CompleteReloadOnServer(UNGEquipmentInstance* Source);
	void BindInventory(UNGInventoryManagerComponent* Inventory);
	void BindAbilitySystem(UAbilitySystemComponent* ASC);
	void SetWeaponMesh(USkeletalMeshComponent* Mesh);
	FSimpleMulticastDelegate OnPresentationChanged;
	UFUNCTION(BlueprintCallable) void RequestEquip(bool bEquip);
	UFUNCTION(BlueprintCallable) void RequestFire();
	void UnequipOnServer();
	// Called by NGFireAbility after GAS checks state tags and commits the fire cooldown.
	bool FireOnServer(UNGEquipmentInstance* Source);
	UFUNCTION(NetMulticast, Unreliable) void MulticastReload(UNGWeaponDefinition* ReloadWeapon, int32 Revision);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, int32 ShotId,UNGWeaponDefinition* ShotWeapon);
	UFUNCTION(NetMulticast, Unreliable) void MulticastProjectileImpact(FVector_NetQuantize Point,FVector_NetQuantizeNormal Normal,int32 ShotId,UNGWeaponDefinition* ShotWeapon,bool bCharacter);
	// Shared damage entry point rejects stale Pawn/ASC pairings, including corpses.
	static bool ApplyDamageToPawn(APawn* Target, UAbilitySystemComponent* Source, float Damage, const FHitResult* Hit = nullptr);
private:
	UNGEquipmentComponent* GetEquipmentComponent() const;
	UAbilitySystemComponent* GetCombatASC() const { return ObservedASC.Get(); }
	TWeakObjectPtr<USkeletalMeshComponent> WeaponMesh;
	void EquipmentChanged();
	UFUNCTION(Server, Reliable) void ServerRequestFire(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection, int32 EquipmentRevision);
	// Only valid during synchronous activation of GA_Fire; never reused by a later shot.
	TOptional<FTransform> RequestedShotView;
	TWeakObjectPtr<UNGInventoryItemInstance> BoundItem;
	TWeakObjectPtr<UNGInventoryManagerComponent> BoundInventory;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	FDelegateHandle AimingTagHandle;
	int32 ShotSerial = 0;
	bool bCompletingReload=false;
	UFUNCTION() void OnRep_Equipment();
	UFUNCTION() void OnRep_Ammo();
	UFUNCTION() void OnRep_CurrentItem();
	void AimingTagChanged(FGameplayTag Tag, int32 Count);
	UFUNCTION() void InventoryChanged();
};
