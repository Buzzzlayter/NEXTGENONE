#include "Gameplay/AI/States/NGAIFireBurstState.h"
#include "Gameplay/Combat/NGWeaponActor.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
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

bool UNGAIFireBurstState::HasClearShot(const APawn& Pawn, const AActor& Target, const FVector& TargetLocation, const FVector& ShotOrigin)
{
	const UWorld* World = Pawn.GetWorld();
	if (!World)
		return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGAIFire), false, &Pawn);
	TArray<AActor*> Attached;
	Pawn.GetAttachedActors(Attached);
	Params.AddIgnoredActors(Attached);
	const auto ClearToTarget = [&](const FVector& Start)
	{
		FHitResult Hit;
		return !World->LineTraceSingleByChannel(Hit, Start, TargetLocation, ECC_Visibility, Params)
			|| Hit.GetActor() == &Target;
	};
	return ClearToTarget(Pawn.GetPawnViewLocation()) && ClearToTarget(ShotOrigin);
}

void UNGAIFireBurstState::StartAction()
{
	ShotsFired = 0;
	const auto* Target = ActionController->GetMemory()->GetTarget();
	Combat = ExpectedPawn->FindComponentByClass<UNGCombatComponent>();
	Weapon = Combat.IsValid() ? Combat->GetWeaponInstance() : nullptr;
	const auto* Equipment = ExpectedPawn->FindComponentByClass<UNGEquipmentComponent>();
	if (!Target || !Target->bVisible || !Weapon.IsValid() || !Equipment || !Equipment->IsCurrent(Weapon.Get())
		|| Combat->GetMagazineAmmo() <= 0
		|| !FMath::IsFinite(BurstDuration) || BurstDuration < .1f || BurstDuration >= ActionTimeout
		|| !FMath::IsFinite(AimSettleTime) || AimSettleTime < 0.f || AimSettleTime >= BurstDuration
		|| !FMath::IsFinite(AimToleranceDegrees) || AimToleranceDegrees < 0.f || AimToleranceDegrees > 45.f)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Burst needs a visible target and loaded equipped weapon")); return; }
	BurstTarget = Target->Actor;
	ActionController->SetFocus(BurstTarget.Get(), FireFocusPriority);
	bOwnsFireFocus = true;
	AimReadyTime = GetWorld()->GetTimeSeconds() + AimSettleTime;
	const uint64 StartedRun = RunSerial;
	if (!AimInput.Start(this, ActionASC.Get(), TAG_NGInputAim, BurstDuration,
		[this, StartedRun]() { return IsCurrentRun(StartedRun); },
		[this]() { return HasCurrentAvatar(); }, [this](bool bCancelled) { AimEnded(bCancelled); }))
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Aim activation rejected")); return; }
	if (!bRunning) return;
	CooldownHandle = ActionASC->RegisterGameplayTagEvent(TAG_NGFireCooldown, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::FireCooldownChanged);
	GetWorld()->GetTimerManager().SetTimer(FireTimer, FTimerDelegate::CreateWeakLambda(this, [this, StartedRun]()
	{
		if (IsCurrentRun(StartedRun)) UpdateFire();
	}), .05f, true);
}

void UNGAIFireBurstState::UpdateFire()
{
	const auto* Equipment = ExpectedPawn->FindComponentByClass<UNGEquipmentComponent>();
	const auto* Target = ActionController->GetMemory()->GetTarget();
	if (!Target || Target->Actor != BurstTarget || !Target->bVisible || !Weapon.IsValid() || !Equipment || !Equipment->IsCurrent(Weapon.Get())
		|| !Combat.IsValid() || Combat->GetWeaponInstance() != Weapon.Get()
		|| !ActionASC->HasMatchingGameplayTag(TAG_NGWeaponAiming))
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Burst target, weapon or aiming lost")); return; }
	if (GetWorld()->GetTimeSeconds() < AimReadyTime || ActionASC->HasMatchingGameplayTag(TAG_NGFireCooldown)) return;
	if (Combat->GetMagazineAmmo() <= 0)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Magazine empty")); return; }
	const auto* WeaponActor = Combat->GetWeaponActor();
	if (!WeaponActor)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Weapon actor unavailable")); return; }
	const FVector Eye = ExpectedPawn->GetPawnViewLocation();
	const FVector Direction = (Target->LastSeenLocation - Eye).GetSafeNormal();
	if (FVector::DotProduct(Direction, ActionController->GetControlRotation().Vector())
		< FMath::Cos(FMath::DegreesToRadians(AimToleranceDegrees))) return;
	if (!UNGAIFireBurstState::HasClearShot(*ExpectedPawn.Get(), *BurstTarget.Get(), Target->LastSeenLocation, WeaponActor->GetShotOrigin()))
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
	if (!Combat.IsValid() || !Weapon.IsValid() || !Equipment || !Equipment->IsCurrent(Weapon.Get()))
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Weapon removed during shot")); return; }
	if (Combat->GetMagazineAmmo() >= AmmoBefore)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("GAS or combat rejected shot")); return; }
	++ShotsFired;
}

void UNGAIFireBurstState::FireCooldownChanged(FGameplayTag Tag, int32 Count)
{
	if (Count == 0 && bRunning && HasCurrentAvatar()) UpdateFire();
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
	if (auto* ASC = ActionASC.Get())
		ASC->RegisterGameplayTagEvent(TAG_NGFireCooldown, EGameplayTagEventType::NewOrRemoved).Remove(CooldownHandle);
	CooldownHandle.Reset();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(FireTimer);
	if (bOwnsFireFocus && ActionController.IsValid()) ActionController->ClearFocus(FireFocusPriority);
	bOwnsFireFocus = false;
	BurstTarget.Reset(); Combat.Reset(); Weapon.Reset();
	AimInput.Reset();
}
