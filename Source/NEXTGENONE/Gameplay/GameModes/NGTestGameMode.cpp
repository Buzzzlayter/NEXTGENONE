#include "Gameplay/GameModes/NGTestGameMode.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Player/NextgenonePlayerController.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Abilities/NGAimAbility.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

void ANGCombatTestGameMode::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke")))
    {
        Deadline = GetWorld()->GetTimeSeconds() + 120.;
        GetWorldTimerManager().SetTimer(SmokeTimer, this, &ThisClass::SmokeTick, .1f, true);
    }
#endif
}
void ANGCombatTestGameMode::Fail(const FString& Reason)
{
    UE_LOG(LogTemp, Error, TEXT("NG_SMOKE_FAIL phase=%d %s"), Phase, *Reason);
    ExpectedReport = -1; Phase = -1;
    GetWorldTimerManager().ClearTimer(SmokeTimer);
}
void ANGCombatTestGameMode::RecordSmoke(ANextgenonePlayerController* PC, int32 Command, bool bPassed)
{
    if (Phase < 0 || !FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke")) || Command != ExpectedReport || !Players.Contains(PC)) return;
    if (!bPassed) { Fail(FString::Printf(TEXT("client %s report %d failed"), *GetNameSafe(PC), Command)); return; }
    if (!Reports.Contains(PC)) UE_LOG(LogTemp, Display, TEXT("NG_CLIENT_REPORT command=%d player=%s passed=1"), Command, *GetNameSafe(PC));
    Reports.Add(PC);
}
void ANGCombatTestGameMode::AskClients(int32 Command)
{
    Reports.Reset(); ExpectedReport = Command;
    FNGCombatSmokeRequest Request;
    Request.Command = Command; Request.ExpectedHealth = ExpectedHealth; Request.ExpectedAmmo = ExpectedAmmo;
    Request.Shooter = Cast<ANextgenoneCharacter>(Players[0]->GetPawn());
    Request.Target = Corpse.IsValid() ? Corpse.Get() : Cast<ANextgenoneCharacter>(Players[1]->GetPawn());
    for (auto PC : Players) if (PC.IsValid()) PC->ClientSmoke(Request);
    PhaseTime = GetWorld()->GetTimeSeconds();
}
void ANGCombatTestGameMode::SendShooter(int32 Command)
{
    FNGCombatSmokeRequest Request;
    Request.Command = Command; Request.ActionSlot = ActionSlot;
    Request.Target = Cast<ANextgenoneCharacter>(Players[1]->GetPawn());
    Players[0]->ClientSmoke(Request);
}
void ANGCombatTestGameMode::SmokeTick()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now > Deadline) { Fail(TEXT("timeout waiting for configured fixtures, readiness, camera alignment or replication")); return; }
    if (Phase == 0)
    {
        Players.Reset();
        for (TActorIterator<ANextgenonePlayerController> It(GetWorld()); It; ++It) if (It->GetPawn()) Players.Add(*It);
        if (Players.Num() != 2) return;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            auto* Pawn = Cast<ANextgenoneCharacter>(Players[Index]->GetPawn());
            if (!Pawn || !Pawn->CharacterStateMachine->IsGameplayReady(TAG_NGAlive) || Pawn->GetMaxHealth() <= 0.f) return;
        }
        for (int32 Index = 0; Index < 2; ++Index)
        {
            auto* Pawn = Cast<ANextgenoneCharacter>(Players[Index]->GetPawn());
            Pawn->SetActorLocation(FVector(Index * 600.f, 0, 100));
            Pawn->SetActorRotation(FRotator::ZeroRotator);
            Pawn->GetCharacterMovement()->DisableMovement();
        }
        AskClients(0); Phase = 1; return;
    }
    if (Players.Num() != 2 || !Players[0].IsValid() || !Players[1].IsValid()) { Fail(TEXT("player disconnected")); return; }
    auto* A = Cast<ANextgenoneCharacter>(Players[0]->GetPawn());
    auto* B = Cast<ANextgenoneCharacter>(Players[1]->GetPawn());
    if (!A || !B) { if (Phase == 7) return; Fail(TEXT("missing Pawn")); return; }
    if (Phase == 1)
    {
        if (Reports.Num() != 2) { if (Now - PhaseTime > 1.) AskClients(0); return; }
        auto* Player = A->GetPlayerState<ANextgenonePlayerState>();
        if (!Player || !Player->Inventory->HasResolvedItems()) return;
        for (int32 Ordinal = 0; ; ++Ordinal)
        {
            const int32 Slot = Player->Inventory->FindSlotByTag(TAG_NGSlotAction, Ordinal);
            if (Slot == INDEX_NONE) break;
            auto* Item = Player->Inventory->GetItemAtSlot(Slot);
            if (Item && UNGCombatComponent::GetWeaponDefinition(GetDefault<UNGInventoryItemDefinition>(Item->GetItemDef())))
            { ActionSlot = Ordinal; break; }
        }
        if (ActionSlot == INDEX_NONE) { Fail(TEXT("fixture requires Inventory.Layout and a starter weapon in Action; configure test GameMode PlayerStateClass")); return; }
        SendShooter(1); Phase = 2; PhaseTime = Now; return;
    }
    if (Phase == 2)
    {
        const auto* Def = A->Combat->GetEquipment().Definition.Get();
        if (!A->Combat->GetEquipment().bEquipped || !Def) return;
        FString Error;
        if (!Def->ValidateDefinition(Error) || Def->PelletsPerShot != 1 || Def->SpreadDegrees != 0.f)
        { Fail(TEXT("fixture requires a valid single-projectile weapon with zero spread")); return; }
        ShotDamage = Def->Damage;
        ExpectedHealth = B->GetHealth(); ExpectedAmmo = A->Combat->GetMagazineAmmo();
        if (ExpectedHealth <= 0.f || ExpectedAmmo < FMath::CeilToInt(ExpectedHealth / ShotDamage))
        { Fail(TEXT("fixture requires enough magazine ammo to kill target without reload")); return; }
        ShotWait = FMath::Max(.5f, Def->FireInterval + .2f + 600.f / Def->MuzzleSpeed);
        // Repeat initialization, then inspect the actual equipment input grants rather than whole-ASC size.
        A->TryInitializeGameplay(); A->TryInitializeGameplay();
        for (const FName TagName : {FName(TEXT("Input.Combat.Fire")), FName(TEXT("Input.Combat.Reload")), FName(TEXT("Input.Aim"))})
        {
            int32 Count = 0;
            const auto Tag = FGameplayTag::RequestGameplayTag(TagName);
            for (const auto& Spec : A->GetAbilitySystemComponent()->GetActivatableAbilities())
                if (Spec.GetDynamicSpecSourceTags().HasTagExact(Tag)) ++Count;
            if (Count != 1) { Fail(TEXT("duplicate/missing equipment input grants")); return; }
        }
        // Probe the authoritative gate directly. FireOnServer without a fresh remote view is invalid.
        auto* ASC = Cast<UNGPlayerAbilitySystemComponent>(A->GetAbilitySystemComponent());
        const float ProbeInterval = FMath::Max(.25f, Def->FireInterval);
        if (!ASC || !ASC->TryCommitFire(A, ProbeInterval) || ASC->TryCommitFire(A, ProbeInterval))
        { Fail(TEXT("immediate limiter reactivation accepted")); return; }
        A->Combat->SetEquippedOnServer(false); A->Combat->SetEquippedOnServer(true);
        if (ASC->CanFireNow(A)) { Fail(TEXT("reequip reset persistent fire limiter")); return; }
        UE_LOG(LogTemp, Display, TEXT("NG_LIMITER_PASS direct_server_gate reactivation=blocked reequip=blocked"));
        SendShooter(7); // RequestFire before ADS must leave ammo/health unchanged.
        Phase = 3; PhaseTime = Now; Reports.Reset(); ExpectedReport = 2; return;
    }
    if (Phase == 3)
    {
        if (Now - PhaseTime < ShotWait) return;
        if (!FMath::IsNearlyEqual(B->GetHealth(), ExpectedHealth) || A->Combat->GetMagazineAmmo() != ExpectedAmmo)
        { Fail(TEXT("hip fire changed health/ammo")); return; }
        if (!Reports.Contains(Players[0]) || !A->Combat->IsAiming()) { SendShooter(2); return; }
        SendShooter(3); ++Shots; --ExpectedAmmo; ExpectedHealth = FMath::Max(0.f, ExpectedHealth - ShotDamage);
        Phase = 4; PhaseTime = Now; return;
    }
    if (Phase == 4)
    {
        if (Now - PhaseTime < ShotWait) return;
        if (!FMath::IsNearlyEqual(B->GetHealth(), ExpectedHealth) || A->Combat->GetMagazineAmmo() != ExpectedAmmo)
        { Fail(FString::Printf(TEXT("shot %d expected hp=%.2f/ammo=%d actual hp=%.2f/ammo=%d"), Shots, ExpectedHealth, ExpectedAmmo, B->GetHealth(), A->Combat->GetMagazineAmmo())); return; }
        AskClients(ExpectedHealth <= 0.f ? 5 : 4); Phase = 5; return;
    }
    if (Phase == 5)
    {
        if (Reports.Num() != 2) { if (Now - PhaseTime > 1.) AskClients(ExpectedHealth <= 0.f ? 5 : 4); return; }
        if (ExpectedHealth > 0.f)
        {
            SendShooter(3); ++Shots; --ExpectedAmmo; ExpectedHealth = FMath::Max(0.f, ExpectedHealth - ShotDamage);
            Phase = 4; PhaseTime = Now; return;
        }
        Corpse = B;
        Players[1]->UnPossess(); RestartPlayer(Players[1].Get());
        Phase = 7; PhaseTime = Now; return;
    }
    if (Phase == 7)
    {
        auto* OldPawn = Corpse.Get();
        if (!OldPawn || B == OldPawn || !B->CharacterStateMachine->IsGameplayReady(TAG_NGAlive)) return;
        const float Health = B->GetHealth();
        if (Health <= 0.f || !FMath::IsNearlyEqual(Health, B->GetMaxHealth()) || OldPawn->GetAbilitySystemComponent())
        { Fail(TEXT("respawn binding")); return; }
        OldPawn->TryInitializeGameplay(); OldPawn->UnbindGameplay();
        if (UNGCombatComponent::ApplyDamageToPawn(OldPawn, A->GetAbilitySystemComponent(), ShotDamage))
        { Fail(TEXT("corpse accepted damage")); return; }
        auto Context = A->GetAbilitySystemComponent()->MakeEffectContext(); Context.AddSourceObject(OldPawn);
        auto StaleSpec = A->GetAbilitySystemComponent()->MakeOutgoingSpec(UNGDamageEffect::StaticClass(), 1.f, Context);
        if (!StaleSpec.IsValid()) { Fail(TEXT("damage spec unavailable")); return; }
        StaleSpec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.Damage")), ShotDamage);
        A->GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*StaleSpec.Data, B->GetAbilitySystemComponent());
        if (!FMath::IsNearlyEqual(B->GetHealth(), Health) || !B->GetAbilitySystemComponent())
        { Fail(TEXT("old body affected new Avatar")); return; }
        AskClients(6); Phase = 8; return;
    }
    if (Phase == 8)
    {
        if (Reports.Num() != 2) { if (Now - PhaseTime > 1.) AskClients(6); return; }
        UE_LOG(LogTemp, Display, TEXT("NG_SMOKE_PASS action_item GAS_ADS fresh_client_view ammo damage health death respawn corpse_guard; limiter=direct_gate_probe shots=%d"), Shots);
        GetWorldTimerManager().ClearTimer(SmokeTimer); Phase = 9; ExpectedReport = -1;
    }
}
