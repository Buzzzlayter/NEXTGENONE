#include "Gameplay/Abilities/NGGameplayAbilityFromWeapon.h"
#include "AbilitySystemComponent.h"
#include "Gameplay/Combat/NGWeaponActor.h"
#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "GameFramework/Pawn.h"

UNGGameplayAbilityFromWeapon::UNGGameplayAbilityFromWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNGGameplayAbilityFromWeapon::ResolveWeaponContext(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* Info, FNGWeaponAbilityContext& OutContext) const
{
	OutContext = {};
	const auto* ASC = Info ? Info->AbilitySystemComponent.Get() : nullptr;
	const auto* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
	if (!ASC || !Avatar || ASC->GetAvatarActor() != Avatar) return false;
	const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	if (!Spec || !Spec->Ability || Spec->PendingRemove) return false;
	auto* Source = Cast<UNGEquipmentInstance>(Spec->SourceObject.Get());
	const auto* Equipment = Avatar->FindComponentByClass<UNGEquipmentComponent>();
	if (!Equipment || !Equipment->IsCurrent(Source)) return false;
	auto* Weapon = Cast<ANGWeaponActor>(Equipment->GetVisualActor(Source->GetSlot()));
	auto* Definition = Cast<UNGWeaponDefinition>(Source->GetDefinition());
	if (!Weapon || !Definition || Weapon->GetEquipmentSource() != Source) return false;
	OutContext = {Source, Weapon, Definition};
	return true;
}

bool UNGGameplayAbilityFromWeapon::CanActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* Info, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	FNGWeaponAbilityContext Context;
	return ResolveWeaponContext(Handle, Info, Context)
		&& Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags);
}
