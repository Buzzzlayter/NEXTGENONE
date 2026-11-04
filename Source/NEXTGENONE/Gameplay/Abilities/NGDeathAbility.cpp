#include "Gameplay/Abilities/NGDeathAbility.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "Character/NGControlStateDefinition.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNGDeadStateEffect::UNGDeadStateEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    FInheritedTagContainer Tags; Tags.AddTag(TAG_NGDead);
    auto* Component = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DeadTags"));
    GEComponents.Add(Component); Component->SetAndApplyTargetTagChanges(Tags);
}

UNGDeathAbility::UNGDeathAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
}

bool UNGDeathAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
    if (!Info || !Info->IsNetAuthority() || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
    return IsValidStateAvatar(Info, Handle);
}

bool UNGDeathAbility::IsValidStateAvatar(const FGameplayAbilityActorInfo* Info, FGameplayAbilitySpecHandle Handle) const
{
    if (!Info || !Info->IsNetAuthority()) return false;
    AActor* Pawn = Info->AvatarActor.Get();
    const auto* ASC = Info->AbilitySystemComponent.Get();
    const auto* Machine = Pawn ? Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>() : nullptr;
    const auto* Definition = Machine ? Machine->GetAbilityStateDefinition(ASC, Handle) : nullptr;
    return ASC && ASC->GetAvatarActor() == Pawn && ASC->HasMatchingGameplayTag(TAG_NGDead) && Machine &&
        Machine->HasStateTag(TAG_NGDead) && Definition && Definition->PresentationCue;
}

void UNGDeathAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
    if (!IsValidStateAvatar(Info, Handle))
    {
        EndAbility(Handle, Info, Activation, true, true);
        return;
    }
    DeathPawn = Info->AvatarActor;
    DeathASC = Info->AbilitySystemComponent;
    const auto* Machine = DeathPawn->FindComponentByClass<UNGCharacterStateMachineComponent>();
    const auto* Definition = Machine->GetAbilityStateDefinition(DeathASC.Get(), Handle);
    PresentationTag = Definition ? Definition->RegisterPresentationCue() : FGameplayTag();
    Info->AbilitySystemComponent->CancelAbilities(nullptr, nullptr, this);
    HandleDeath();
}

void UNGDeathAbility::HandleDeath_Implementation()
{
    StartDeathRagdoll();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UNGDeathAbility::StartDeathRagdoll()
{
    AActor* Pawn = DeathPawn.Get();
    if (!Pawn || !Pawn->HasAuthority() || !PresentationTag.IsValid()) return;
    FGameplayCueParameters Params; Params.SourceObject = Pawn;
    if (auto* ASC = DeathASC.Get(); ASC && ASC->GetAvatarActor() == Pawn)
        ASC->ExecuteGameplayCue(PresentationTag, Params);
    else
        UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(Pawn, PresentationTag, EGameplayCueEvent::Executed, Params);
}

void UNGDeathAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
    if (!IsActive()) return;
    // Default presentation already requested it; only interrupted sequences need finalization here.
    if (bCancelled) StartDeathRagdoll();
    DeathPawn.Reset(); DeathASC.Reset(); PresentationTag = FGameplayTag();
    Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}
