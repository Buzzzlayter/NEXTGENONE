#include "Gameplay/Player/NextgenonePlayerState.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"

ANextgenonePlayerState::ANextgenonePlayerState()
{
    AbilitySystem = CreateDefaultSubobject<UNGPlayerAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    HealthSet = CreateDefaultSubobject<UNGHealthSet>(TEXT("HealthSet"));
    Inventory = CreateDefaultSubobject<UNGInventoryManagerComponent>(TEXT("Inventory"));
    SetNetUpdateFrequency(30.f);
}

bool ANextgenonePlayerState::StartLife(ANextgenoneCharacter* Pawn)
{
    if (!HasAuthority() || bChangingLife || !IsValid(Pawn) || Pawn->IsDead() ||
        Pawn->GetPlayerState<ANextgenonePlayerState>() != this || (GetPawn() && GetPawn() != Pawn)) return false;

    AActor* Previous = AbilitySystem->GetAvatarActor();
    auto* PreviousMachine = Previous ? Previous->FindComponentByClass<UNGCharacterStateMachineComponent>() : nullptr;
    if (Previous && Previous != Pawn && PreviousMachine && !PreviousMachine->CanReleaseAvatar())
    {
        if (PendingLifePawn != Pawn) UE_LOG(LogTemp, Display, TEXT("%s: deferring %s until %s finishes state resource entry."), *GetName(), *Pawn->GetName(), *Previous->GetName());
        PendingLifePawn = Pawn;
        return false;
    }
    if (InitializedPawn == Pawn && AbilitySystem->GetAvatarActor() == Pawn) return true;
    TGuardValue<bool> LifeGuard(bChangingLife, true);
    if (PreviousMachine) PreviousMachine->OnResourcesReady.RemoveAll(this);
    if (AActor* PriorAvatar = AbilitySystem->GetAvatarActor(); PriorAvatar && PriorAvatar != Pawn)
        if (auto* Machine = PriorAvatar->FindComponentByClass<UNGCharacterStateMachineComponent>()) Machine->SuspendResources();
    // Cancel old actions before changing actor info: callbacks must never operate on the next Avatar.
    AbilitySystem->ClearAbilityInput();
    AbilitySystem->CancelAllAbilities();
    const bool bNewLife = InitializedPawn != Pawn;
    if (bNewLife) { InitializedPawn = Pawn; ++LifeSerial; }
    AbilitySystem->InitAbilityActorInfo(this, Pawn);
    Pawn->CharacterStateMachine->OnResourcesReady.AddUObject(this, &ThisClass::StateResourcesReady);
    if (bNewLife)
        AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UNGInitHealthEffect>(), 1.f, AbilitySystem->MakeEffectContext());
    PendingLifePawn.Reset();
    ForceNetUpdate();
    return true;
}

void ANextgenonePlayerState::StateResourcesReady(AActor* Avatar)
{
    auto* Pawn = Cast<ANextgenoneCharacter>(Avatar);
    if (!HasAuthority() || !Pawn || AbilitySystem->GetAvatarActor() != Pawn || !Pawn->CharacterStateMachine->CanReleaseAvatar()) return;
    if (!Pawn->GetController()) Pawn->UnbindGameplay();
    ResumePendingLife(Pawn);
}

void ANextgenonePlayerState::ResumePendingLife(ANextgenoneCharacter* PreviousPawn)
{
    if (!HasAuthority() || !PreviousPawn) return;
    if (AbilitySystem->GetAvatarActor() && AbilitySystem->GetAvatarActor() != PreviousPawn) return;
    ANextgenoneCharacter* Pending = PendingLifePawn.Get();
    PendingLifePawn.Reset();
    if (IsValid(Pending) && GetPawn() == Pending && Pending->GetPlayerState<ANextgenonePlayerState>() == this)
        Pending->TryInitializeGameplay();
}

void ANextgenonePlayerState::EndPlay(EEndPlayReason::Type Reason)
{
    PendingLifePawn.Reset();
    if (HasAuthority())
        if (AActor* Avatar = AbilitySystem->GetAvatarActor())
            if (auto* Machine = Avatar->FindComponentByClass<UNGCharacterStateMachineComponent>())
            { Machine->OnResourcesReady.RemoveAll(this); Machine->SuspendResources(); }
    Super::EndPlay(Reason);
}
