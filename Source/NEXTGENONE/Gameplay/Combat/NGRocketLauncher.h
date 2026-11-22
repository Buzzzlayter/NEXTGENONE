#pragma once

#include "Gameplay/Combat/NGWeaponActor.h"
#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "NGRocketLauncher.generated.h"

class ANGRocketProjectile;

USTRUCT()
struct FNGRocketAmmoVariant
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Rocket Ammo")
	TSoftClassPtr<UNGInventoryItemDefinition> ItemClass;

	UPROPERTY(EditDefaultsOnly, Category="Rocket Ammo", meta=(Categories="Weapon.LoadedAmmo.Rocket"))
	FGameplayTag LoadedTag;

	UPROPERTY(EditDefaultsOnly, Category="Rocket Ammo")
	TSubclassOf<ANGRocketProjectile> RocketClass;
};

UCLASS(BlueprintType, Blueprintable, PrioritizeCategories=("Rocket Launcher"))
class NEXTGENONE_API UNGRocketLauncherDefinition : public UNGWeaponDefinition
{
	GENERATED_BODY()

public:
	UNGRocketLauncherDefinition();
	virtual bool ValidateDefinition(FString& Error) const override;
	virtual void GetReloadAmmoOptions(TArray<FNGWeaponAmmoOption>& OutOptions) const override;

	UPROPERTY(EditDefaultsOnly, Category="Rocket Launcher")
	TSubclassOf<ANGRocketProjectile> RocketClass;

	UPROPERTY(EditDefaultsOnly, Category="Rocket Launcher")
	TArray<FNGRocketAmmoVariant> SpecialAmmo;
};

UCLASS(Blueprintable)
class NEXTGENONE_API ANGRocketLauncher : public ANGWeaponActor
{
	GENERATED_BODY()

protected:
	virtual bool ExecuteFire_Implementation(FVector ViewOrigin, FVector ViewDirection) override;
	virtual bool PlaysFireSoundAtMuzzle() const override { return false; }
};
