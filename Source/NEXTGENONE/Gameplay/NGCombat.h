#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NGAbilitySystem.h"
#include "NGCombat.generated.h"

class UStaticMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class NEXTGENONE_API UNGWeaponDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
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

UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGCombatComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(EditDefaultsOnly, Category="Weapon") TObjectPtr<UNGWeaponDefinition> TestWeapon;
	UPROPERTY(BlueprintAssignable) FNGEquipmentChanged OnEquipmentChanged;
	UPROPERTY(BlueprintAssignable) FNGAmmoChanged OnAmmoChanged;
	// Weapon-pack adapters subscribe here; cosmetic events never apply damage.
	UPROPERTY(BlueprintAssignable) FNGShotCosmetic OnShotCosmetic;
	UFUNCTION(BlueprintPure) FNGEquipmentState GetEquipment() const { return Equipment; }
	UFUNCTION(BlueprintPure) int32 GetMagazineAmmo() const { return MagazineAmmo; }
	UFUNCTION(BlueprintCallable) void RequestEquip(bool bEquip);
	UFUNCTION(BlueprintCallable) void RequestFire();
	void SetEquippedOnServer(bool bEquip);
	bool FireOnServer();
	UFUNCTION(Server, Reliable) void ServerSetEquipped(bool bEquip);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, int32 ShotId);
	// Shared damage entry point rejects stale Pawn/ASC pairings, including corpses.
	static bool ApplyDamageToPawn(class ANextgenoneCharacter* Target, UAbilitySystemComponent* Source, float Damage, const FHitResult* Hit = nullptr);
private:
	UPROPERTY(ReplicatedUsing=OnRep_Equipment) FNGEquipmentState Equipment;
	UPROPERTY(ReplicatedUsing=OnRep_Ammo) int32 MagazineAmmo = 0;
	UPROPERTY() TObjectPtr<UNGAbilitySet> WeaponAbilities;
	UPROPERTY() FNGGrantedHandles WeaponHandles;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WeaponVisual;
	TWeakObjectPtr<UAbilitySystemComponent> GrantedASC;
	bool bMagazineInitialized = false;
	int32 ShotSerial = 0;
	UFUNCTION() void OnRep_Equipment();
	UFUNCTION() void OnRep_Ammo();
	UFUNCTION() void HandleDeath(bool bDead);
};

UCLASS()
class NEXTGENONE_API UNGFireAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGFireAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
};
