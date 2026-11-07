#include "Gameplay/Player/NextgenonePlayerController.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Abilities/NGAimAbility.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "Gameplay/GameModes/NGTestGameMode.h"
#include "EngineUtils.h"
#include "Gameplay/UI/NGSettingsPanel.h"
#include "Input/NGGameplayInputComponent.h"
#include "Components/ALSDebugComponent.h"
#include "GameFramework/PlayerState.h"

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
void ANextgenonePlayerController::ClientSmoke_Implementation(FNGCombatSmokeRequest Request)
{
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke"))) return;
    auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn());
    if (!ControlledPawn) return; // Server retries within a bounded global deadline.
    auto* ASC = Cast<UNGAbilitySystemComponent>(ControlledPawn->GetAbilitySystemComponent());
    const int32 Command = Request.Command;
    if (Command == 0 || Command == 6)
    {
        if (ASC && ASC->GetOwnerActor() == PlayerState && ASC->GetAvatarActor() == ControlledPawn
            && ControlledPawn->CharacterStateMachine->IsGameplayReady(TAG_NGAlive)
            && ControlledPawn->GetMaxHealth() > 0.f && FMath::IsNearlyEqual(ControlledPawn->GetHealth(), ControlledPawn->GetMaxHealth()))
            ServerSmokeResult(Command, true);
        return;
    }
    if (Command == 1) { ControlledPawn->Combat->RequestSelectSlot(Request.ActionSlot); return; }
    if (Command == 2)
    {
        if (!ASC || !IsValid(Request.Target)) return;
        ASC->AbilityInputTagPressed(TAG_NGInputAim);
        FVector Origin, Direction;
        if (!ControlledPawn->Aim->GetViewRay(Origin, Direction)) return;
        const FVector TargetPoint = Request.Target->GetActorLocation() + FVector(0, 0, 20);
        const FVector ToTarget = (TargetPoint - Origin).GetSafeNormal();
        // Camera interpolation updates on subsequent frames; retry until the actual view ray converges.
        SetControlRotation(ToTarget.Rotation());
        if (ControlledPawn->Combat->IsAiming() && FVector::DotProduct(Direction, ToTarget) > .99999f)
            ServerSmokeResult(Command, true);
        return;
    }
    if (Command == 3 || Command == 7) { ControlledPawn->Combat->RequestFire(); return; }
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
void ANextgenonePlayerController::ServerSmokeResult_Implementation(int32 Command, bool bPassed)
{
#if !UE_BUILD_SHIPPING
	if (auto* Mode = GetWorld()->GetAuthGameMode<ANGCombatTestGameMode>()) Mode->RecordSmoke(this, Command, bPassed);
#endif
}
