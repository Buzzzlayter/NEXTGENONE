#include "NGCharacterStateMachineComponent.h"
#include "NGCharacterStateResources.h"
#include "AbilitySystemComponent.h"
#include "GCFSMUtilities.h"
#include "GCFSM.h"
#include "GameFramework/Pawn.h"

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
}

bool UNGCharacterStateMachineComponent::QueueServerEvent(FGameplayTag Event)
{
	if (bEnding || !GetOwner()->HasAuthority() || !Event.IsValid() || MachineName.IsNone() || !UGCFSMUtilities::IsFSMRunning(GetOwner(), MachineName)) return false;
	GetOwner()->FlushNetDormancy();
	UGCFSMUtilities::TriggerEvent(GetOwner(), Event.GetTagName(), EGCFSMTriggerEventTargetPolicy::ContextObject, true, false, EGCFSMTriggerEventQueuePolicy::QueueUnlessAlreadyInQueue);
	GetOwner()->ForceNetUpdate();
	return true;
}

void UNGCharacterStateMachineComponent::NotifyDeath() { if (GetOwner()->HasAuthority()) QueueServerEvent(DeathEvent); }
void UNGCharacterStateMachineComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority()) FinalizeProxyInitialization();
	StartIfReady(); RefreshResources();
}

void UNGCharacterStateMachineComponent::RegisterResources(UNGCharacterStateResources* Resources) { ActiveResources.AddUnique(Resources); }
void UNGCharacterStateMachineComponent::UnregisterResources(UNGCharacterStateResources* Resources) { ActiveResources.Remove(Resources); }

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
