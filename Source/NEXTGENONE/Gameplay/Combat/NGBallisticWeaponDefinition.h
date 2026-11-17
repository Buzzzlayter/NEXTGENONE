#pragma once

#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "NGBallisticWeaponDefinition.generated.h"

class ANGBallisticProjectile;

UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Ballistic Weapon Definition"))
class NEXTGENONE_API UNGBallisticWeaponDefinition : public UNGWeaponDefinition
{
	GENERATED_BODY()
public:
	UNGBallisticWeaponDefinition();
	virtual bool ValidateDefinition(FString& Error) const override;
	virtual void ResolveResources(TArray<FNGEquipmentAbilityGrant>& OutAbilities, TArray<TSubclassOf<UGameplayEffect>>& OutEffects) const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics") TSubclassOf<ANGBallisticProjectile> ProjectileClass;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="100",ClampMax="200000")) float MuzzleSpeed=60000.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="0",ClampMax="10")) float GravityScale=1.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="0",ClampMax="2")) float LinearDrag=0.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="1",ClampMax="16")) int32 PelletsPerShot=1;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="0",ClampMax="20")) float SpreadDegrees=0.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ballistics",meta=(ClampMin="0",ClampMax="10000")) float BodyImpactImpulse=1600.f;
};
