#include "Gameplay/Combat/NGDamage.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "GameplayTags/NGGameplayTags.h"
#include "GameFramework/Pawn.h"

UAbilitySystemComponent* NGDamage::FindLivingAbilitySystem(APawn* Target)
{
	if (!IsValid(Target)) return nullptr;
	auto* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC || TargetASC->GetAvatarActor() != Target || TargetASC->HasMatchingGameplayTag(TAG_NGDead)
		|| TargetASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f) return nullptr;
	return TargetASC;
}

bool NGDamage::ApplyToPawn(APawn* Target, UAbilitySystemComponent* Source, float Damage, const FHitResult* Hit)
{
	if (!Source || !Source->IsOwnerActorAuthoritative() || !FMath::IsFinite(Damage) || Damage <= 0) return false;
	auto* TargetASC = FindLivingAbilitySystem(Target);
	if (!TargetASC || !Target->HasAuthority()) return false;
	auto Context = Source->MakeEffectContext(); Context.AddSourceObject(Target);
	if (Hit) Context.AddHitResult(*Hit);
	auto Spec = Source->MakeOutgoingSpec(UNGDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid()) return false;
	Spec.Data->SetSetByCallerMagnitude(TAG_NGDamage, Damage);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
	return true;
}
