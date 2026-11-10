#include "Gameplay/Abilities/NGGameplayAbilityFromWeapon.h"
#include "AbilitySystemComponent.h"
#include "Gameplay/Combat/NGCombat.h"
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
	if (!ASC || ASC->GetAvatarActor() != Info->AvatarActor.Get()) return false;
	const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	if (!Spec || !Spec->Ability || Spec->PendingRemove) return false;
	auto* Source = Cast<UNGEquipmentInstance>(Spec->SourceObject.Get());
	if (!Source || !Source->IsCurrent() || Source->GetAvatar() != Info->AvatarActor.Get()) return false;
	auto* Combat = Source->GetAvatar()->FindComponentByClass<UNGCombatComponent>();
	auto* Definition = Cast<UNGWeaponDefinition>(Source->GetDefinition());
	if (!Combat || !Definition || Combat->GetWeaponInstance() != Source) return false;
	OutContext = {Source, Combat, Definition};
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
