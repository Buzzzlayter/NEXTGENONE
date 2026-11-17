#include "Gameplay/AbilitySystem/NGLaserSightCue.h"
#include "Gameplay/Combat/NGWeaponActor.h"
#include "GameplayTags/NGGameplayTags.h"
#include "GameFramework/Pawn.h"

namespace
{
	bool SetObservedLaser(AActor* Target, const FGameplayCueParameters& Parameters, bool bVisible)
	{
		const APawn* Pawn = Cast<APawn>(Target);
		if (!Pawn || Pawn->HasAuthority() || Pawn->IsLocallyControlled()) return false;

		auto* Weapon = Cast<ANGWeaponActor>(Parameters.EffectContext.GetSourceObject());
		if (!IsValid(Weapon) || Weapon->GetOwner() != Pawn) return false;
		Weapon->SetLaserSightVisible(bVisible);
		return true;
	}
}

UNGLaserSightCue::UNGLaserSightCue(const FObjectInitializer& Init) : Super(Init)
{
	GameplayCueTag = TAG_NGLaserSightCue;
}

bool UNGLaserSightCue::WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	return SetObservedLaser(Target, Parameters, true);
}

bool UNGLaserSightCue::OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	return SetObservedLaser(Target, Parameters, false);
}
