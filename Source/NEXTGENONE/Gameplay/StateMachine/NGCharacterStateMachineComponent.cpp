#include "NGCharacterStateMachineComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "NGCharacterStateResources.h"
#include "AbilitySystemComponent.h"
#include "GCFSMUtilities.h"
#include "GCFSM.h"
#include "GameFramework/Pawn.h"
#include "Character/NGControlStateDefinition.h"
#include "GameplayCueManager.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Net/UnrealNetwork.h"

bool UNGCharacterStateMachineComponent::StartIfReady()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (bEnding || bResourcesFailed || MachineName.IsNone() || !Pawn || !Pawn->HasAuthority() || !Pawn->HasActorBegunPlay() || !ReadyASC.IsValid() || ReadyASC->GetAvatarActor() != Pawn) return false;
	if (!UGCFSMUtilities::IsFSMRunning(Pawn, MachineName))
	{
		TArray<UGCFSMReplicationComponent*> ReplicationComponents;
		Pawn->GetComponents(ReplicationComponents);
		if (ReplicationComponents.Num() != 1)
		{ UE_LOG(LogTemp, Error, TEXT("%s must have exactly one GCFSM replication component (CharacterStateMachine)."), *Pawn->GetName()); return false; }
		Pawn->FlushNetDormancy();
		UGCFSMUtilities::LaunchFSM(Pawn, MachineName, EGCFSMReplicationOptions::Replicated);
	}
	return UGCFSMUtilities::IsFSMRunning(Pawn, MachineName);
}

void UNGCharacterStateMachineComponent::NotifyGameplayReady(UAbilitySystemComponent* ASC)
{
	ReadyASC = ASC;
	if (bResourcesFailed) { OnRep_ResourceFailure(); return; }
	StartIfReady(); RefreshResources();
	if (RequestedStateEvent.IsValid()) QueueServerEvent(RequestedStateEvent);
}

bool UNGCharacterStateMachineComponent::QueueServerEvent(FGameplayTag Event)
{
	if (bEnding || bResourcesFailed || !GetOwner()->HasAuthority() || !Event.IsValid() || MachineName.IsNone() || !UGCFSMUtilities::IsFSMRunning(GetOwner(), MachineName)) return false;
	GetOwner()->FlushNetDormancy();
	UGCFSMUtilities::TriggerEvent(GetOwner(), Event.GetTagName(), EGCFSMTriggerEventTargetPolicy::ContextObject, true, false, EGCFSMTriggerEventQueuePolicy::QueueUnlessAlreadyInQueue);
	GetOwner()->ForceNetUpdate();
	return true;
}

void UNGCharacterStateMachineComponent::NotifyDeath()
{
	if (bEnding || !GetOwner()->HasAuthority()) return;
	if (!bResourcesFailed) { RequestStateTransition(TAG_NGDead, DeathEvent); return; }
	// A broken graph cannot run GA_Death, but a later lethal hit must still mark this body dead.
	FailedStateTag = TAG_NGDead;
	GetOwner()->FlushNetDormancy(); GetOwner()->ForceNetUpdate();
	NotifyStateChanged();
}

void UNGCharacterStateMachineComponent::RequestStateTransition(FGameplayTag StateTag, FGameplayTag Event)
{
	if (bEnding || bResourcesFailed || !GetOwner()->HasAuthority() || !StateTag.IsValid() || !Event.IsValid() || HasStateTag(StateTag)) return;
	RequestedStateTag = StateTag;
	RequestedStateEvent = Event;
	NotifyStateChanged();
	StartIfReady();
	if (RequestedStateEvent.IsValid()) QueueServerEvent(RequestedStateEvent);
}
void UNGCharacterStateMachineComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority()) FinalizeProxyInitialization();
	StartIfReady(); RefreshResources();
}

void UNGCharacterStateMachineComponent::RegisterResources(UNGCharacterStateResources* Resources) { ActiveResources.AddUnique(Resources); NotifyStateChanged(); }
void UNGCharacterStateMachineComponent::UnregisterResources(UNGCharacterStateResources* Resources) { ActiveResources.Remove(Resources); NotifyStateChanged(); }

FGameplayTag UNGCharacterStateMachineComponent::GetCurrentStateTag() const
{
	if (RequestedStateTag.IsValid()) return RequestedStateTag;
	if (bResourcesFailed) return FailedStateTag;
	for (int32 Index = ActiveResources.Num() - 1; Index >= 0; --Index)
		if (ActiveResources[Index].IsValid() && ActiveResources[Index]->Definition) return ActiveResources[Index]->Definition->StateTag;
	return {};
}

bool UNGCharacterStateMachineComponent::HasStateTag(FGameplayTag StateTag) const
{
	if (!StateTag.IsValid()) return false;
	if (bResourcesFailed) return FailedStateTag.MatchesTag(StateTag);
	if (RequestedStateTag.MatchesTag(StateTag)) return true;
	for (const auto& Resource : ActiveResources)
		if (Resource.IsValid() && Resource->Definition && Resource->Definition->StateTag.MatchesTag(StateTag)) return true;
	return false;
}

void UNGCharacterStateMachineComponent::NotifyStateChanged()
{
	const FGameplayTag Current = GetCurrentStateTag();
	if (Current == LastNotifiedStateTag) return;
	const FGameplayTag Previous = LastNotifiedStateTag;
	LastNotifiedStateTag = Current;
	OnStateChanged.Broadcast(Previous, Current);
}

bool UNGCharacterStateMachineComponent::CanReleaseAvatar() const
{
	if (bEnding || bResourcesFailed || !GetReadyASC()) return true;
	if (RequestedStateTag.IsValid()) return false;
	for (const auto& Resource : ActiveResources)
		if (Resource.IsValid() && !Resource->IsEntryReady()) return false;
	return true;
}

void UNGCharacterStateMachineComponent::NotifyResourcesReady(UNGCharacterStateResources* Resources)
{
	if (bEnding || bResourcesFailed || !Resources || !Resources->IsEntryReady() || !ActiveResources.Contains(Resources)) return;
	if (Resources->Definition && Resources->Definition->StateTag == RequestedStateTag)
	{
		RequestedStateTag = FGameplayTag(); RequestedStateEvent = FGameplayTag();
	}
	NotifyStateChanged();
	RestorePresentation();
	if (CanReleaseAvatar()) OnResourcesReady.Broadcast(GetOwner());
}

void UNGCharacterStateMachineComponent::NotifyResourceFailure(FGameplayTag StateTag, const FString& Reason)
{
	if (bEnding || bResourcesFailed) return;
	bResourcesFailed = true; // Terminal for this Pawn; never retry partially entered gameplay.
	FailedStateTag = RequestedStateTag.IsValid() ? RequestedStateTag : StateTag;
	RequestedStateTag = FGameplayTag();
	RequestedStateEvent = FGameplayTag();
	UE_LOG(LogTemp, Error, TEXT("Character FSM resource failure: pawn=%s machine=%s state=%s: %s. Resources released; Avatar handoff allowed."),
		*GetNameSafe(GetOwner()), *MachineName.ToString(), *FailedStateTag.ToString(), *Reason);
	GetOwner()->FlushNetDormancy(); GetOwner()->ForceNetUpdate();
	OnRep_ResourceFailure();
}

void UNGCharacterStateMachineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, bResourcesFailed);
	DOREPLIFETIME(ThisClass, FailedStateTag);
}

void UNGCharacterStateMachineComponent::OnRep_ResourceFailure()
{
	if (!bResourcesFailed || bEnding) return;
	RequestedStateTag = {}; RequestedStateEvent = {};
	SuspendResources();
	if (auto* ASC = GetReadyASC())
	{
		if (auto* GameplayASC = Cast<UNGAbilitySystemComponent>(ASC)) GameplayASC->ClearAbilityInput();
		ASC->CancelAllAbilities();
	}
	OnResourceFailure.Broadcast();
	NotifyStateChanged();
	// This notification means the handoff barrier is released, not gameplay success.
	OnResourcesReady.Broadcast(GetOwner());
}

void UNGCharacterStateMachineComponent::RestorePresentation() const
{
	if (bResourcesFailed || GetOwner()->HasAuthority() || !GetOwner()->HasActorBegunPlay()) return;
	for (const auto& Resource : ActiveResources)
		if (Resource.IsValid() && Resource->Definition)
		{
			const FGameplayTag Cue = Resource->Definition->RegisterPresentationCue();
			if (Cue.IsValid()) UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), Cue, EGameplayCueEvent::Executed, FGameplayCueParameters());
		}
}

const UNGControlStateDefinition* UNGCharacterStateMachineComponent::GetAbilityStateDefinition(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const
{
	for (const auto& Resource : ActiveResources)
		if (Resource.IsValid() && Resource->OwnsAbility(ASC, Handle)) return Resource->Definition;
	return nullptr;
}

UAbilitySystemComponent* UNGCharacterStateMachineComponent::GetReadyASC() const
{
	auto* ASC = ReadyASC.Get();
	return ASC && ASC->GetAvatarActor() == GetOwner() ? ASC : nullptr;
}

bool UNGCharacterStateMachineComponent::OwnsStateAbility(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const
{
	for (const auto& Resource : ActiveResources)
		if (Resource.IsValid() && Resource->OwnsAbility(ASC, Handle)) return true;
	return false;
}

bool UNGCharacterStateMachineComponent::IsGameplayReady(FGameplayTag RequiredState) const
{
	if (bEnding || bResourcesFailed || RequestedStateTag.IsValid() || !GetReadyASC() || ActiveResources.IsEmpty()) return false;
	bool bHasRequiredState = !RequiredState.IsValid();
	for (const auto& Resource : ActiveResources)
	{
		if (!Resource.IsValid() || !Resource->Definition || !Resource->IsEntryReady()) return false;
		// State replication and owner-only ability specs can arrive in either order.
		for (const auto& Grant : Resource->Definition->Abilities)
		{
			bool bFound = false;
			for (const auto& Spec : GetReadyASC()->GetActivatableAbilities())
				if (Spec.Ability && Spec.Ability->GetClass() == Grant.Ability.Get()) { bFound = true; break; }
			if (!bFound) return false;
		}
		bHasRequiredState |= Resource->Definition->StateTag.MatchesTag(RequiredState);
	}
	return bHasRequiredState;
}

void UNGCharacterStateMachineComponent::RefreshResources()
{
	if (bEnding || bResourcesFailed) return;
	for (const auto& Resource : TArray<TWeakObjectPtr<UNGCharacterStateResources>>(ActiveResources))
		if (Resource.IsValid()) Resource->RefreshResources();
	ActiveResources.RemoveAll([](const auto& Entry) { return !Entry.IsValid(); });
}

void UNGCharacterStateMachineComponent::SuspendResources()
{
	for (const auto& Resource : TArray<TWeakObjectPtr<UNGCharacterStateResources>>(ActiveResources))
		if (Resource.IsValid()) Resource->ReleaseResources();
}

void UNGCharacterStateMachineComponent::EndPlay(EEndPlayReason::Type Reason)
{
	bEnding = true;
	SuspendResources(); ActiveResources.Reset(); ReadyASC.Reset();
	// Stop() asserts if a GCFSM state is active. Pause the dying owner's ticker instead.
	if (UGCFSMUtilities::GetRootStateFromContext(GetOwner(), false)) UGCFSMUtilities::PauseFSMs(GetOwner(), true);
	Super::EndPlay(Reason);
}
