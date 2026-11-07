#include "Gameplay/Debug/NGCombatSmokeComponent.h"

#include "Gameplay/Player/NextgenonePlayerController.h"
#include "Gameplay/Inventory/NGPlayerInventoryManagerComponent.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "Gameplay/GameModes/NGTestGameMode.h"
#include "GameplayTags/NGGameplayTags.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

UNGCombatSmokeComponent::UNGCombatSmokeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNGCombatSmokeComponent::ClientSmoke_Implementation(FNGCombatSmokeRequest Request)
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke"))) return;
	auto* PC = Cast<APlayerController>(GetOwner());
	auto* ControlledPawn = PC ? Cast<ANextgenoneCharacter>(PC->GetPawn()) : nullptr;
	if (!ControlledPawn) return; // Server retries within a bounded global deadline.
	auto* ASC = Cast<UNGAbilitySystemComponent>(ControlledPawn->GetAbilitySystemComponent());
	const int32 Command = Request.Command;
	if (Command == 0 || Command == 6)
	{
		if (ASC && ASC->GetOwnerActor() == PC->PlayerState && ASC->GetAvatarActor() == ControlledPawn
			&& ControlledPawn->CharacterStateMachine->IsGameplayReady(TAG_NGAlive)
			&& ControlledPawn->GetMaxHealth() > 0.f && FMath::IsNearlyEqual(ControlledPawn->GetHealth(), ControlledPawn->GetMaxHealth()))
			ServerSmokeResult(Command, true);
		return;
	}
	if (Command == 1) { if (auto* Inventory = PC->PlayerState ? PC->PlayerState->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr) Inventory->RequestActivationSlot(Request.ActionSlot); return; }
	if (Command == 2)
	{
		if (!ASC || !IsValid(Request.Target) || !ControlledPawn->Combat->GetEquipment().bEquipped
			|| !ControlledPawn->Combat->GetEquippedItem()) return;
		if (!ControlledPawn->Combat->IsAiming())
		{
			// A rejected activation still latches Spec.InputPressed; retry a complete input cycle.
			ASC->AbilityInputTagReleased(TAG_NGInputAim);
			ASC->AbilityInputTagPressed(TAG_NGInputAim);
		}
		FVector Origin, Direction;
		if (!ControlledPawn->Aim->GetViewRay(Origin, Direction)) return;
		const FVector TargetPoint = Request.Target->GetActorLocation() + FVector(0, 0, 20);
		const FVector ToTarget = (TargetPoint - Origin).GetSafeNormal();
		// Camera interpolation updates on subsequent frames; retry until the actual view ray converges.
		PC->SetControlRotation(ToTarget.Rotation());
		if (ControlledPawn->Combat->IsAiming() && FVector::DotProduct(Direction, ToTarget) > .99999f)
			ServerSmokeResult(Command, true);
		return;
	}
	if (Command == 3) { ControlledPawn->Combat->RequestFire(); return; }
	if (Command == 7)
	{
		if (!ASC || !ControlledPawn->Combat->GetEquipment().bEquipped || !ControlledPawn->Combat->GetEquippedItem()) return;
		if (ControlledPawn->Combat->IsAiming()) { ASC->AbilityInputTagReleased(TAG_NGInputAim); return; }
		ControlledPawn->Combat->RequestFire();
		ServerSmokeResult(Command, true);
		return;
	}
	if (Command == 4 || Command == 5)
	{
		if (!IsValid(Request.Target) || !IsValid(Request.Shooter)) return;
		const bool bTargetCorrect = FMath::IsNearlyEqual(Request.Target->GetHealth(), Request.ExpectedHealth)
			&& Request.Target->IsDead() == (Request.ExpectedHealth <= 0.f);
		const bool bAmmoCorrect = ControlledPawn != Request.Shooter || ControlledPawn->Combat->GetMagazineAmmo() == Request.ExpectedAmmo;
		if (bTargetCorrect && bAmmoCorrect && Request.Shooter->Combat->GetEquipment().bEquipped)
			ServerSmokeResult(Command, true);
	}
#endif
}
void UNGCombatSmokeComponent::ServerSmokeResult_Implementation(int32 Command, bool bPassed)
{
#if !UE_BUILD_SHIPPING
	if (auto* Mode = GetWorld()->GetAuthGameMode<ANGCombatTestGameMode>()) Mode->RecordSmoke(Cast<ANextgenonePlayerController>(GetOwner()), Command, bPassed);
#endif
}
