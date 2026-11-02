#include "NextgenonePlayerState.h"
#include "NGPlayerAbilitySystem.h"
#include "NextgenoneCharacter.h"
#include "Input/NGCharacterInputComponent.h"
#include "Character/NGCharacterDefinition.h"
#include "StateMachine/NGCharacterStateMachineComponent.h"

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
    UNGCharacterDefinition* Definition = Pawn->GameplayInput ? Pawn->GameplayInput->CharacterDefinition.Get() : nullptr;
    FString Error;
    if (!Definition || !Definition->ValidateAbilityGrants(Error))
    {
        UE_LOG(LogTemp, Error, TEXT("%s: assign a valid GameplayInput.CharacterDefinition on %s. %s"), *GetName(), *Pawn->GetName(), *Error);
        return false;
    }
    if (GrantedPawn == Pawn)
    {
        if (GrantedDefinition != Definition || AbilitySystem->GetAvatarActor() != Pawn)
        {
            UE_LOG(LogTemp, Error, TEXT("%s: active CharacterDefinition/Avatar changed outside the lifecycle; release before rebinding."), *GetName());
            return false;
        }
        return true;
    }
    TGuardValue<bool> LifeGuard(bChangingLife, true);
    if (AActor* PriorAvatar = AbilitySystem->GetAvatarActor(); PriorAvatar && PriorAvatar != Pawn)
        if (auto* Machine = PriorAvatar->FindComponentByClass<UNGCharacterStateMachineComponent>()) Machine->SuspendResources();
    if (ANextgenoneCharacter* PreviousGrantPawn = GrantedPawn.Get()) ReleaseCharacterAbilities(PreviousGrantPawn);
    else { CharacterHandles.Remove(AbilitySystem); GrantedDefinition.Reset(); }
    // Cancel old actions before changing actor info: callbacks must never operate on the next Avatar.
    AbilitySystem->ClearAbilityInput();
    AbilitySystem->CancelAllAbilities();
    const bool bNewLife = InitializedPawn != Pawn;
    if (bNewLife) { InitializedPawn = Pawn; ++LifeSerial; }
    AbilitySystem->InitAbilityActorInfo(this, Pawn);
    AbilitySystem->SetLooseGameplayTagCount(TAG_NGDead, 0, EGameplayTagReplicationState::TagOnly);
    if (bNewLife)
        AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UNGInitHealthEffect>(), 1.f, AbilitySystem->MakeEffectContext());
    if (!Definition->GiveAbilitySets(AbilitySystem, Pawn, CharacterHandles, bNewLife)) return false;
    GrantedPawn = Pawn;
    GrantedDefinition = Definition;
    ForceNetUpdate();
    return true;
}

void ANextgenonePlayerState::ReleaseCharacterAbilities(ANextgenoneCharacter* ExpectedPawn)
{
    if (!HasAuthority() || !ExpectedPawn || GrantedPawn != ExpectedPawn) return;
    GrantedPawn.Reset(); GrantedDefinition.Reset();
    CharacterHandles.Remove(AbilitySystem);
}

void ANextgenonePlayerState::EndPlay(EEndPlayReason::Type Reason)
{
    if (HasAuthority()) CharacterHandles.Remove(AbilitySystem);
    GrantedPawn.Reset(); GrantedDefinition.Reset();
    Super::EndPlay(Reason);
}
