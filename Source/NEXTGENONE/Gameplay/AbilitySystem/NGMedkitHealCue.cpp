#include "Gameplay/AbilitySystem/NGMedkitHealCue.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayTags/NGGameplayTags.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UNGMedkitHealCue::UNGMedkitHealCue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameplayCueTag = TAG_NGMedkitHealCue;
}

bool UNGMedkitHealCue::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	if (!IsValid(Target) || Parameters.EffectCauser.Get() != Target) return false;
	auto* EffectSystem = const_cast<UNiagaraSystem*>(Cast<UNiagaraSystem>(Parameters.GetSourceObject()));
	auto* TargetRoot = Target->GetRootComponent();
	if (!IsValid(EffectSystem) || !IsValid(TargetRoot)) return false;

	return UNiagaraFunctionLibrary::SpawnSystemAttached(EffectSystem, TargetRoot, NAME_None, FVector::ZeroVector,
		FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, true) != nullptr;
}
