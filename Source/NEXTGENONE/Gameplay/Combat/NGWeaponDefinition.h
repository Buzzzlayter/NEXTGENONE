#pragma once

#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Library/ALSCharacterEnumLibrary.h"
#include "NGWeaponDefinition.generated.h"

class UNGInventoryItemDefinition;
class ANGBallisticProjectile;
class UGameplayAbility;
class UAnimSequence;
class UAnimMontage;
class UStaticMesh;
class USoundBase;
class UNiagaraSystem;

UCLASS(BlueprintType, Blueprintable)
class NEXTGENONE_API UNGWeaponDefinition : public UNGEquipmentDefinition
{
	GENERATED_BODY()
public:
	UNGWeaponDefinition();
	virtual bool ValidateDefinition(FString& Error) const override;
	virtual bool ValidateItemDefinition(const UNGInventoryItemDefinition& Item, FString& Error) const override;
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) EALSOverlayState Overlay = EALSOverlayState::Default;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bAutomatic = false;
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
