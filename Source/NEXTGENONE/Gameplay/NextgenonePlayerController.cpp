#include "NextgenonePlayerController.h"
#include "NextgenoneCharacter.h"
#include "NGCombat.h"
#include "NGTestGameMode.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EngineUtils.h"

void ANextgenonePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	auto* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input) return;
	if (!CombatMapping)
	{
		CombatMapping = NewObject<UInputMappingContext>(this);
		FireAction = NewObject<UInputAction>(this); FireAction->ValueType = EInputActionValueType::Boolean;
		EquipAction = NewObject<UInputAction>(this); EquipAction->ValueType = EInputActionValueType::Boolean;
		UnequipAction = NewObject<UInputAction>(this); UnequipAction->ValueType = EInputActionValueType::Boolean;
		CombatMapping->MapKey(FireAction, EKeys::LeftMouseButton);
		CombatMapping->MapKey(EquipAction, EKeys::E);
		CombatMapping->MapKey(UnequipAction, EKeys::U);
	}
	Input->BindAction(FireAction, ETriggerEvent::Started, this, &ThisClass::NGFire);
	Input->BindAction(EquipAction, ETriggerEvent::Started, this, &ThisClass::NGEquip);
	Input->BindAction(UnequipAction, ETriggerEvent::Started, this, &ThisClass::NGUnequip);
	UpdateGameplayPawn();
}
void ANextgenonePlayerController::UpdateGameplayPawn()
{
	if (auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn())) ControlledPawn->TryInitializeGameplay();
	if (CombatMapping && IsLocalController())
		if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer())) Sub->AddMappingContext(CombatMapping, 2);
}
void ANextgenonePlayerController::RemoveCombatMapping()
{
	if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		if (CombatMapping) Sub->RemoveMappingContext(CombatMapping);
}
void ANextgenonePlayerController::OnPossess(APawn* ControlledPawn) { Super::OnPossess(ControlledPawn); UpdateGameplayPawn(); }
void ANextgenonePlayerController::OnRep_Pawn() { Super::OnRep_Pawn(); if (GetPawn()) UpdateGameplayPawn(); else RemoveCombatMapping(); }
void ANextgenonePlayerController::EndPlay(const EEndPlayReason::Type Reason) { RemoveCombatMapping(); Super::EndPlay(Reason); }
void ANextgenonePlayerController::NGEquip() { if (auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn())) ControlledPawn->Combat->RequestEquip(true); }
void ANextgenonePlayerController::NGUnequip() { if (auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn())) ControlledPawn->Combat->RequestEquip(false); }
void ANextgenonePlayerController::NGFire() { if (auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn())) ControlledPawn->Combat->RequestFire(); }
void ANextgenonePlayerController::ClientSmoke_Implementation(int32 Command)
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke"))) return;
	auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn());
	if ((Command == 0 || Command == 6) && (!ControlledPawn || !ControlledPawn->GetAbilitySystemComponent()))
	{
		// Controller RPC can precede Pawn/PlayerState replication on a new connection.
		FTimerHandle Retry;
		GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this, [this, Command]() { ClientSmoke_Implementation(Command); }), .2f, false);
		return;
	}
	if (!ControlledPawn) { ServerSmokeResult(Command, false); return; }
	if (Command == 0 || Command == 6)
	{
		auto* ASC = ControlledPawn->GetAbilitySystemComponent();
		ServerSmokeResult(Command, ASC && ASC->GetOwnerActor() == PlayerState && ASC->GetAvatarActor() == ControlledPawn && IsLocalController() && ControlledPawn->GetHealth()==100.f);
	}
	else if (Command == 1) { SetControlRotation(FRotator::ZeroRotator); NGEquip(); }
	else if (Command == 2)
	{
		// Actual client GAS activation requests, interleaved with equip RPCs.
		for (int32 i = 0; i < 8; ++i) { NGFire(); NGUnequip(); NGEquip(); }
		NGFire();
	}
	else if (Command == 3) NGFire();
	else
	{
		bool bFoundState = false;
		for (TActorIterator<ANextgenoneCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsDead()) { bFoundState |= Command == 5 && It->GetHealth() == 0.f; }
			else if (It->GetHealth() == 75.f) bFoundState |= Command == 4;
		}
		// All clients must also see an equipped shooter; only owner sees ammo.
		bool bEquippedSeen = false;
		for (TActorIterator<ANextgenoneCharacter> It(GetWorld()); It; ++It) bEquippedSeen |= It->Combat->GetEquipment().bEquipped;
		const bool bOwnerAmmoCorrect = !ControlledPawn->Combat->GetEquipment().bEquipped || ControlledPawn->Combat->GetMagazineAmmo() == (Command == 4 ? 11 : 8);
		ServerSmokeResult(Command, bFoundState && bEquippedSeen && bOwnerAmmoCorrect);
	}
#endif
}
void ANextgenonePlayerController::ServerSmokeResult_Implementation(int32 Command, bool bPassed)
{
#if !UE_BUILD_SHIPPING
	if (auto* Mode = GetWorld()->GetAuthGameMode<ANGCombatTestGameMode>()) Mode->RecordSmoke(this, Command, bPassed);
#endif
}
