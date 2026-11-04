#include "Gameplay/Player/NextgenonePlayerController.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/GameModes/NGTestGameMode.h"
#include "EngineUtils.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Gameplay/UI/NGSettingsPanel.h"
#include "Input/NGGameplayInputComponent.h"
#include "Components/ALSDebugComponent.h"

void ANextgenonePlayerController::BeginPlay() { Super::BeginPlay(); if (IsLocalController()) GetDefault<UNGInputPreferences>()->ApplyAudio(GetWorld()); }
void ANextgenonePlayerController::AddYawInput(float Value) { Super::AddYawInput(Value*GetDefault<UNGInputPreferences>()->LookSensitivity); }
void ANextgenonePlayerController::AddPitchInput(float Value)
{
    const auto* Preferences = GetDefault<UNGInputPreferences>();
    Super::AddPitchInput(Value * Preferences->LookSensitivity * (Preferences->bInvertY ? -1.f : 1.f));
}
void ANextgenonePlayerController::SetupInputComponent()
{
    // ALS auto-binds by action name and adds its own contexts. The Pawn now owns gameplay bindings.
    APlayerController::SetupInputComponent();
}
void ANextgenonePlayerController::RefreshPawnPresentation()
{
	if (APawn* PriorGameplayPawn = PreviousInputPawn.Get(); PriorGameplayPawn && PriorGameplayPawn != GetPawn())
		if (auto* Input = PriorGameplayPawn->FindComponentByClass<UNGGameplayInputComponent>()) Input->ShutdownInput();
	PreviousInputPawn = GetPawn();
    PossessedCharacter = Cast<AALSBaseCharacter>(GetPawn());
    if (GetNetMode() != NM_DedicatedServer) SetupCamera();
    if (PossessedCharacter)
        if (auto* Debug = PossessedCharacter->FindComponentByClass<UALSDebugComponent>()) Debug->OnPlayerControllerInitialized(this);
}
void ANextgenonePlayerController::OnPossess(APawn* ControlledPawn)
{
    APlayerController::OnPossess(ControlledPawn);
    RefreshPawnPresentation();
}
void ANextgenonePlayerController::OnRep_Pawn()
{
    APlayerController::OnRep_Pawn();
    RefreshPawnPresentation();
}
void ANextgenonePlayerController::ToggleInventoryUI()
{
    if (const auto* PS = GetPlayerState<ANextgenonePlayerState>())
    {
        InventoryUIRequested.Broadcast(PS->Inventory);
        OnInventoryRequested(PS->Inventory);
    }
}
void ANextgenonePlayerController::RequestDropItem(UNGInventoryItemInstance* Item,int32 Count)
{
	if (!IsLocalController() && !HasAuthority()) return;
	auto* Expected=Cast<ANextgenoneCharacter>(GetPawn());
	if (HasAuthority()) ServerDropItem_Implementation(Expected,Item,Count); else ServerDropItem(Expected,Item,Count);
}
void ANextgenonePlayerController::ServerDropItem_Implementation(ANextgenoneCharacter* ExpectedPawn,UNGInventoryItemInstance* Item,int32 Count)
{
	if (!ExpectedPawn || GetPawn()!=ExpectedPawn) return;
	auto* System=GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	auto* PS=GetPlayerState<ANextgenonePlayerState>();
	if (!System || !System->IsCurrentInteractor(ExpectedPawn) || !PS) return;
	const double Now=GetWorld()->GetTimeSeconds(); if (Now<PS->NextAllowedDropTime) return;
	PS->NextAllowedDropTime=Now+.15; System->DropInventoryItem(ExpectedPawn,Item,Count);
}
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
	else if (Command == 1) { SetControlRotation(FRotator::ZeroRotator); ControlledPawn->Combat->RequestEquip(true); }
	else if (Command == 2)
	{
		// Actual client GAS activation requests, interleaved with equip RPCs.
		for (int32 i = 0; i < 8; ++i) { ControlledPawn->Combat->RequestFire(); ControlledPawn->Combat->RequestEquip(false); ControlledPawn->Combat->RequestEquip(true); }
		ControlledPawn->Combat->RequestFire();
	}
	else if (Command == 3) ControlledPawn->Combat->RequestFire();
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
