#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/Abilities/NGGameplayAbilityFromEquipment.h"
#include "Library/ALSCharacterEnumLibrary.h"
#include "NGCombat.generated.h"

class ANextgenoneCharacter;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UNGInventoryItemInstance;
class UNGInventoryItemDefinition;
class UNGInventoryManagerComponent;
class ANGBallisticProjectile;
class USoundBase;
class UNiagaraSystem;
struct FNGInventoryChangeMessage;

UCLASS(BlueprintType, Blueprintable)
class NEXTGENONE_API UNGWeaponDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UNGWeaponDefinition();
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<USkeletalMesh> SkeletalMesh;
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName AttachSocket = TEXT("hand_r");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FTransform VisualTransform = FTransform(FRotator::ZeroRotator, FVector(12, 0, 0), FVector(0.35, 0.08, 0.08));
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGPunchStarted, FVector, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNGPunchHit, ANextgenoneCharacter*, Target, FVector, ImpactPoint);
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(EditDefaultsOnly, Category="Weapon") TArray<TSoftClassPtr<UNGInventoryItemDefinition>> StarterItems;
	UPROPERTY(BlueprintAssignable) FNGEquipmentChanged OnEquipmentChanged;
	UPROPERTY(BlueprintAssignable) FNGAmmoChanged OnAmmoChanged;
	// Weapon-pack adapters subscribe here; cosmetic events never apply damage.
	UPROPERTY(BlueprintAssignable) FNGShotCosmetic OnShotCosmetic;
	UPROPERTY(BlueprintAssignable) FNGProjectileImpact OnProjectileImpact;
	UPROPERTY(BlueprintAssignable, Category="Punch") FNGPunchStarted OnPunchStarted;
	UPROPERTY(BlueprintAssignable, Category="Punch") FNGPunchHit OnPunchHit;
	UPROPERTY(BlueprintAssignable, Category="Weapon") FNGAimChanged OnAimChanged;
	UFUNCTION(BlueprintPure) FNGEquipmentState GetEquipment() const { return Equipment; }
	UFUNCTION(BlueprintPure) int32 GetMagazineAmmo() const;
	UFUNCTION(BlueprintPure) int32 GetReserveAmmo() const;
	UFUNCTION(BlueprintPure) bool IsAiming() const;
	UFUNCTION(BlueprintPure) UNGInventoryItemInstance* GetEquippedItem() const { return CurrentItem; }
	UFUNCTION(BlueprintCallable) void RequestSelectSlot(int32 Index);
	UFUNCTION(BlueprintCallable) void RequestEquipItem(UNGInventoryItemInstance* Item);
	UFUNCTION(BlueprintCallable) void RequestReload();
	bool EquipInventoryItemOnServer(UNGInventoryItemInstance* Item);
	bool CompleteReloadOnServer(UNGInventoryItemInstance* Item);
	void BindInventory();
	void BindAbilitySystem(UAbilitySystemComponent* ASC);
	UFUNCTION(BlueprintCallable) void RequestEquip(bool bEquip);
	UFUNCTION(BlueprintCallable) void RequestFire();
	void StartPrimaryInput();
	void StopPrimaryInput();
	void SetEquippedOnServer(bool bEquip);
	bool FireOnServer();
	UFUNCTION(Server, Reliable) void ServerSetEquipped(bool bEquip);
	UFUNCTION(Server, Reliable) void ServerSelectSlot(int32 Index);
	UFUNCTION(Server, Reliable) void ServerEquipItem(UNGInventoryItemInstance* Item);
	UFUNCTION(NetMulticast, Unreliable) void MulticastReload();
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, int32 ShotId,UNGWeaponDefinition* ShotWeapon);
	UFUNCTION(NetMulticast, Unreliable) void MulticastProjectileImpact(FVector_NetQuantize Point,FVector_NetQuantizeNormal Normal,int32 ShotId,UNGWeaponDefinition* ShotWeapon,bool bCharacter);
	UFUNCTION(NetMulticast, Unreliable) void MulticastPunchStarted(FVector_NetQuantizeNormal Direction, UAnimMontage* Montage);
	UFUNCTION(NetMulticast, Unreliable) void MulticastPunchHit(ANextgenoneCharacter* Target, FVector_NetQuantize ImpactPoint);
	// Shared damage entry point rejects stale Pawn/ASC pairings, including corpses.
	static bool ApplyDamageToPawn(class ANextgenoneCharacter* Target, UAbilitySystemComponent* Source, float Damage, const FHitResult* Hit = nullptr);
private:
	UFUNCTION(Server, Reliable) void ServerRequestFire(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection, int32 EquipmentRevision);
	// Only valid during synchronous activation of GA_Fire; never reused by a later shot.
	TOptional<FTransform> RequestedShotView;
	void ContinuePrimaryInput();
	FTimerHandle PrimaryInputTimer;
	UPROPERTY(ReplicatedUsing=OnRep_Equipment) FNGEquipmentState Equipment;
	UPROPERTY(ReplicatedUsing=OnRep_CurrentItem) TObjectPtr<UNGInventoryItemInstance> CurrentItem;
	UPROPERTY() FNGGrantedHandles WeaponHandles;
	TWeakObjectPtr<UNGInventoryItemInstance> BoundItem;
	TWeakObjectPtr<UNGInventoryManagerComponent> BoundInventory;
	TWeakObjectPtr<UAbilitySystemComponent> GrantedASC;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	FDelegateHandle AimingTagHandle;
	int32 ShotSerial = 0;
	bool bCompletingReload=false;
	UFUNCTION() void OnRep_Equipment();
	UFUNCTION() void OnRep_Ammo();
	UFUNCTION() void OnRep_CurrentItem();
	void AimingTagChanged(FGameplayTag Tag, int32 Count);
	UFUNCTION() void InventoryChanged(FNGInventoryChangeMessage Change);
	UFUNCTION() void HandleDeath(bool bDead);
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGReloadAbility : public UNGGameplayAbilityFromEquipment
{
	GENERATED_BODY()
public:
	UNGReloadAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled) override;
private:
	TWeakObjectPtr<UNGCombatComponent> ReloadCombat;
	TWeakObjectPtr<UNGInventoryItemInstance> ReloadItem;
	UFUNCTION() void FinishReload();
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGFireAbility : public UNGGameplayAbilityFromEquipment
{
	GENERATED_BODY()
public:
	UNGFireAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
};
