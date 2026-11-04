#include "NGCharacterStateMachineComponent.h"
#include "NGCharacterStateResources.h"
#include "AbilitySystemComponent.h"
#include "GCFSMUtilities.h"
#include "GCFSM.h"
#include "GameFramework/Pawn.h"
#include "Character/NGControlStateDefinition.h"
#include "GameplayCueManager.h"
#include "AbilitySystemGlobals.h"

bool UNGCharacterStateMachineComponent::StartIfReady()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (bEnding || MachineName.IsNone() || !Pawn || !Pawn->HasAuthority() || !Pawn->HasActorBegunPlay() || !ReadyASC.IsValid() || ReadyASC->GetAvatarActor() != Pawn) return false;
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
	StartIfReady(); RefreshResources();
	if (RequestedStateEvent.IsValid()) QueueServerEvent(RequestedStateEvent);
}

bool UNGCharacterStateMachineComponent::QueueServerEvent(FGameplayTag Event)
{
	if (bEnding || !GetOwner()->HasAuthority() || !Event.IsValid() || MachineName.IsNone() || !UGCFSMUtilities::IsFSMRunning(GetOwner(), MachineName)) return false;
	GetOwner()->FlushNetDormancy();
	UGCFSMUtilities::TriggerEvent(GetOwner(), Event.GetTagName(), EGCFSMTriggerEventTargetPolicy::ContextObject, true, false, EGCFSMTriggerEventQueuePolicy::QueueUnlessAlreadyInQueue);
	GetOwner()->ForceNetUpdate();
	return true;
}

void UNGCharacterStateMachineComponent::NotifyDeath() { RequestStateTransition(TAG_NGDead, DeathEvent); }

void UNGCharacterStateMachineComponent::RequestStateTransition(FGameplayTag StateTag, FGameplayTag Event)
{
	if (bEnding || !GetOwner()->HasAuthority() || !StateTag.IsValid() || !Event.IsValid() || HasStateTag(StateTag)) return;
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
	for (int32 Index = ActiveResources.Num() - 1; Index >= 0; --Index)
		if (ActiveResources[Index].IsValid() && ActiveResources[Index]->Definition) return ActiveResources[Index]->Definition->StateTag;
	return {};
}

bool UNGCharacterStateMachineComponent::HasStateTag(FGameplayTag StateTag) const
{
	if (!StateTag.IsValid()) return false;
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
	if (bEnding || !GetReadyASC()) return true;
	if (RequestedStateTag.IsValid()) return false;
	for (const auto& Resource : ActiveResources)
		if (Resource.IsValid() && !Resource->IsEntryReady()) return false;
	return true;
}

void UNGCharacterStateMachineComponent::NotifyResourcesReady(UNGCharacterStateResources* Resources)
{
	if (bEnding || !Resources || !Resources->IsEntryReady() || !ActiveResources.Contains(Resources)) return;
	if (Resources->Definition && Resources->Definition->StateTag == RequestedStateTag)
	{
		RequestedStateTag = FGameplayTag(); RequestedStateEvent = FGameplayTag();
	}
	NotifyStateChanged();
	RestorePresentation();
	if (CanReleaseAvatar()) OnResourcesReady.Broadcast(GetOwner());
}

void UNGCharacterStateMachineComponent::RestorePresentation() const
{
	if (GetOwner()->HasAuthority() || !GetOwner()->HasActorBegunPlay()) return;
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
	if (bEnding || RequestedStateTag.IsValid() || !GetReadyASC() || ActiveResources.IsEmpty()) return false;
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
	if (bEnding) return;
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
