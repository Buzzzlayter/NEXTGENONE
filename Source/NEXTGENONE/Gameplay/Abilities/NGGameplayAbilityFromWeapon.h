#pragma once

#include "Gameplay/Abilities/NGGameplayAbilityFromEquipment.h"
#include "NGGameplayAbilityFromWeapon.generated.h"

class ANGWeaponActor;
class UNGEquipmentInstance;
class UNGWeaponDefinition;

/** A short-lived view of the weapon installation bound to one ability spec. Never cached. */
struct FNGWeaponAbilityContext
{
	UNGEquipmentInstance* Source = nullptr;
	ANGWeaponActor* Weapon = nullptr;
	UNGWeaponDefinition* Definition = nullptr;
};

/** Weapon-only context resolution; every activation remains tied to its installation. */
UCLASS(Abstract)
class NEXTGENONE_API UNGGameplayAbilityFromWeapon : public UNGGameplayAbilityFromEquipment
{
	GENERATED_BODY()
public:
	UNGGameplayAbilityFromWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* RelevantTags = nullptr) const override;
protected:
	/** Resolve the source of this spec, including after a delay, rather than selecting another weapon. */
	bool ResolveWeaponContext(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FNGWeaponAbilityContext& OutContext) const;
};
