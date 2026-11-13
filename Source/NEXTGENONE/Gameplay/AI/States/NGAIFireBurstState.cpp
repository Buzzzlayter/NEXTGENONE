#include "Gameplay/AI/States/NGAIFireBurstState.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "GameplayTags/NGGameplayTags.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace { constexpr EAIFocusPriority::Type FireFocusPriority = EAIFocusPriority::Gameplay + 1; }

UNGAIFireBurstState::UNGAIFireBurstState()
{
	ActionTimeout = 4.f;
}

void UNGAIFireBurstState::StartAction()
{
	ShotsFired = 0;
	const auto* Target = ActionController->GetMemory()->GetTarget();
	Combat = ExpectedPawn->FindComponentByClass<UNGCombatComponent>();
	Weapon = Combat.IsValid() ? Combat->GetWeaponInstance() : nullptr;
	if (!Target || !Target->bVisible || !Weapon.IsValid() || !Weapon->IsCurrent()
		|| Combat->GetMagazineAmmo() <= 0
		|| !FMath::IsFinite(BurstDuration) || BurstDuration < .1f || BurstDuration >= ActionTimeout
		|| !FMath::IsFinite(AimSettleTime) || AimSettleTime < 0.f || AimSettleTime >= BurstDuration
		|| !FMath::IsFinite(AimToleranceDegrees) || AimToleranceDegrees < 0.f || AimToleranceDegrees > 45.f)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Burst needs a visible target and loaded equipped weapon")); return; }
	BurstTarget = Target->Actor;
	ActionController->SetFocus(BurstTarget.Get(), FireFocusPriority);
	bOwnsFireFocus = true;
	EarliestShotTime = GetWorld()->GetTimeSeconds() + AimSettleTime;
	const uint64 StartedRun = RunSerial;
	if (!AimInput.Start(this, ActionASC.Get(), TAG_NGInputAim, BurstDuration,
		[this, StartedRun]() { return IsCurrentRun(StartedRun); },
		[this]() { return HasCurrentAvatar(); }, [this](bool bCancelled) { AimEnded(bCancelled); }))
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Aim activation rejected")); return; }
	if (!bRunning) return;
	GetWorld()->GetTimerManager().SetTimer(FireTimer, FTimerDelegate::CreateWeakLambda(this, [this, StartedRun]()
	{
		if (IsCurrentRun(StartedRun)) UpdateFire();
	}), .05f, true);
}

void UNGAIFireBurstState::UpdateFire()
{
	const auto* Target = ActionController->GetMemory()->GetTarget();
	if (!Target || Target->Actor != BurstTarget || !Target->bVisible || !Weapon.IsValid() || !Weapon->IsCurrent()
		|| !Combat.IsValid() || Combat->GetWeaponInstance() != Weapon.Get()
		|| !ActionASC->HasMatchingGameplayTag(TAG_NGWeaponAiming))
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Burst target, weapon or aiming lost")); return; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < EarliestShotTime) return;
	if (Combat->GetMagazineAmmo() <= 0)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Magazine empty")); return; }
	const auto* Definition = Cast<UNGWeaponDefinition>(Weapon->GetDefinition());
	if (!Definition || !FMath::IsFinite(Definition->FireInterval))
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Invalid weapon cadence")); return; }
	const FVector Eye = ExpectedPawn->GetPawnViewLocation();
	const FVector Direction = (Target->LastSeenLocation - Eye).GetSafeNormal();
	if (FVector::DotProduct(Direction, ActionController->GetControlRotation().Vector())
		< FMath::Cos(FMath::DegreesToRadians(AimToleranceDegrees))) return;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGAIFire), false, ExpectedPawn.Get());
	TArray<AActor*> Attached; ExpectedPawn->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult Hit;
	const auto ClearToTarget = [&](const FVector& Start)
	{
		return !GetWorld()->LineTraceSingleByChannel(Hit, Start, Target->LastSeenLocation, ECC_Visibility, Params)
			|| Hit.GetActor() == BurstTarget.Get();
	};
	const FVector Muzzle = Combat->GetShotOrigin();
	if (!ClearToTarget(Eye) || !ClearToTarget(Muzzle))
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Line of fire blocked")); return; }
	// Use the equipment-granted Fire ability. No direct FireOnServer, damage or ammo writes.
	FGameplayAbilitySpecHandle FireHandle;
	int32 Matches = 0;
	{
		FScopedAbilityListLock Lock(*ActionASC.Get());
		for (const auto& Spec : ActionASC->GetActivatableAbilities())
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(TAG_NGFireInput))
			{
				++Matches;
				if (!Spec.IsActive() && !Spec.InputPressed && Spec.SourceObject.Get() == Weapon.Get()) FireHandle = Spec.Handle;
			}
	}
	if (Matches != 1 || !FireHandle.IsValid())
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Expected one idle weapon Fire ability")); return; }
	const uint64 StartedRun = RunSerial;
	const auto ShotASC = ActionASC;
	const int32 AmmoBefore = Combat->GetMagazineAmmo();
	ActionASC->AbilityInputTagPressed(TAG_NGFireInput);
	// Release even when a synchronous shot kills/unpossesses this Pawn; never release on a new Avatar.
	if (HasCurrentAvatar() && ShotASC.IsValid()) ShotASC->AbilityInputTagReleased(TAG_NGFireInput);
	if (!IsCurrentRun(StartedRun)) return;
	if (!Combat.IsValid() || !Weapon.IsValid() || !Weapon->IsCurrent())
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Weapon removed during shot")); return; }
	if (Combat->GetMagazineAmmo() >= AmmoBefore)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("GAS or combat rejected shot")); return; }
	++ShotsFired;
	EarliestShotTime = Now + FMath::Max(.05f, Definition->FireInterval);
}

void UNGAIFireBurstState::AimEnded(bool bWasCancelled)
{
	if (!bWasCancelled && ShotsFired == 0)
		FinishAction(ENGAIActionResult::Failed, TEXT("Burst ended without firing"));
	else FinishAction(bWasCancelled ? ENGAIActionResult::Aborted : ENGAIActionResult::Succeeded,
		bWasCancelled ? FName(TEXT("Aim cancelled")) : FName(TEXT("Burst completed")));
}

void UNGAIFireBurstState::ReleaseActionResources()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(FireTimer);
	if (bOwnsFireFocus && ActionController.IsValid()) ActionController->ClearFocus(FireFocusPriority);
	bOwnsFireFocus = false;
	BurstTarget.Reset(); Combat.Reset(); Weapon.Reset();
	AimInput.Reset();
}
