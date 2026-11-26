#include "NGCharacterStateMachineComponent.h"
#include "NGCharacterStateResources.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "AbilitySystemGlobals.h"
#include "Character/NGControlStateDefinition.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "GameplayCueManager.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Net/UnrealNetwork.h"

UNGCharacterStateMachineComponent::UNGCharacterStateMachineComponent()
{
	SetIsReplicatedByDefault(true);
}

void UNGCharacterStateMachineComponent::NotifyGameplayReady(UAbilitySystemComponent* ASC)
{
	if (bEnding || !ASC || ASC->GetAvatarActor() != GetOwner()) return;
	const bool bNewASC = ReadyASC.Get() != ASC;
	if (bNewASC)
	{
		if (ReadyASC.IsValid()) SuspendResources();
		ReadyASC = ASC;
	}
	if (HasResourceFailure())
	{
		if (bNewASC)
		{
			if (auto* GameplayASC = Cast<UNGAbilitySystemComponent>(ASC)) GameplayASC->ClearAbilityInput();
			ASC->CancelAllAbilities();
			OnResourcesReady.Broadcast(GetOwner());
		}
		return;
	}

	if (!AbilitySpecsReplicatedHandle.IsValid())
		if (auto* PlayerASC = Cast<UNGPlayerAbilitySystemComponent>(ASC))
			AbilitySpecsReplicatedHandle = PlayerASC->OnAbilitySpecsReplicated.AddUObject(this, &ThisClass::NotifyAbilitySpecsReplicated);
	RefreshResources();
}

void UNGCharacterStateMachineComponent::UnbindAbilitySpecNotifications()
{
	if (auto* ASC = Cast<UNGPlayerAbilitySystemComponent>(ReadyASC.Get())) ASC->OnAbilitySpecsReplicated.Remove(AbilitySpecsReplicatedHandle);
	AbilitySpecsReplicatedHandle.Reset();
}

void UNGCharacterStateMachineComponent::NotifyAbilitySpecsReplicated(const UAbilitySystemComponent* ASC)
{
	if (!bEnding && !HasResourceFailure() && GetReadyASC() == ASC) OnResourcesReady.Broadcast(GetOwner());
}

void UNGCharacterStateMachineComponent::BeginPlay()
{
	Super::BeginPlay();
	OnRep_ControlState();
}

void UNGCharacterStateMachineComponent::NotifyDeath()
{
	if (!bEnding && GetOwner()->HasAuthority()) SetStateTag(TAG_NGDead);
}

void UNGCharacterStateMachineComponent::RequestStateTransition(FGameplayTag StateTag)
{
	if (bEnding || HasResourceFailure() || !GetOwner()->HasAuthority() || !StateTag.IsValid() || HasStateTag(TAG_NGDead)) return;
	SetStateTag(StateTag);
}

void UNGCharacterStateMachineComponent::SetStateTag(FGameplayTag StateTag)
{
	if (ControlState.Tag == StateTag) return;
	GetOwner()->FlushNetDormancy();
	ControlState.Tag = StateTag;
	GetOwner()->ForceNetUpdate();
	OnRep_ControlState();
}

void UNGCharacterStateMachineComponent::OnRep_ControlState()
{
	if (bEnding || !HasBegunPlay()) return;
	if (HasResourceFailure())
	{
		HandleResourceFailure();
		NotifyStateChanged();
		return;
	}
	RefreshResources();
}

bool UNGCharacterStateMachineComponent::HasStateTag(FGameplayTag StateTag) const
{
	return StateTag.IsValid() && ControlState.Tag.MatchesTag(StateTag);
}

void UNGCharacterStateMachineComponent::NotifyStateChanged()
{
	if (ControlState.Tag == LastNotifiedStateTag) return;
	const FGameplayTag Previous = LastNotifiedStateTag;
	LastNotifiedStateTag = ControlState.Tag;
	OnStateChanged.Broadcast(Previous, LastNotifiedStateTag);
}

UNGControlStateDefinition* UNGCharacterStateMachineComponent::FindDefinition(FGameplayTag StateTag) const
{
	UNGControlStateDefinition* Match = nullptr;
	for (UNGControlStateDefinition* Definition : StateDefinitions)
	{
		if (!Definition || Definition->StateTag != StateTag) continue;
		if (Match) return nullptr;
		Match = Definition;
	}
	return Match;
}

bool UNGCharacterStateMachineComponent::CanReleaseAvatar() const
{
	if (bEnding || HasResourceFailure() || !GetReadyASC() || !ControlState.Tag.IsValid()) return true;
	return ActiveResources && ActiveResources->GetDefinition()->StateTag == ControlState.Tag && ActiveResources->IsEntryReady();
}

void UNGCharacterStateMachineComponent::NotifyResourcesReady(UNGCharacterStateResources* Resources)
{
	if (bEnding || HasResourceFailure() || !Resources || Resources != ActiveResources.Get() || !Resources->IsEntryReady()
		|| Resources->GetDefinition()->StateTag != ControlState.Tag) return;
	RestorePresentation();
	OnResourcesReady.Broadcast(GetOwner());
}

void UNGCharacterStateMachineComponent::NotifyResourceFailure(const FString& Reason)
{
	if (bEnding || HasResourceFailure()) return;
	UE_LOG(LogTemp, Error, TEXT("Character state resource failure: pawn=%s state=%s: %s"),
		*GetNameSafe(GetOwner()), *ControlState.Tag.ToString(), *Reason);
	if (GetOwner()->HasAuthority())
	{
		GetOwner()->FlushNetDormancy();
		ControlState.bResourceFailure = true;
		GetOwner()->ForceNetUpdate();
	}
	HandleResourceFailure();
}

void UNGCharacterStateMachineComponent::HandleResourceFailure()
{
	if (bResourceFailureHandled) return;
	bResourceFailureHandled = true;
	const TWeakObjectPtr<UAbilitySystemComponent> FailedASC(GetReadyASC());
	SuspendResources();
	if (auto* ASC = FailedASC.Get(); ASC && ASC->GetAvatarActor() == GetOwner())
	{
		if (auto* GameplayASC = Cast<UNGAbilitySystemComponent>(ASC)) GameplayASC->ClearAbilityInput();
		ASC->CancelAllAbilities();
	}
	OnResourceFailure.Broadcast();
	NotifyStateChanged();
	OnResourcesReady.Broadcast(GetOwner());
}

void UNGCharacterStateMachineComponent::RestorePresentation() const
{
	if (HasResourceFailure() || GetOwner()->HasAuthority() || !GetOwner()->HasActorBegunPlay()
		|| !ActiveResources || !ActiveResources->IsEntryReady()) return;
	const auto* Definition = ActiveResources->GetDefinition();
	if (Definition->StateTag != ControlState.Tag) return;
	const FGameplayTag Cue = Definition->GetPresentationCueTag();
	if (Cue.IsValid()) UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), Cue, EGameplayCueEvent::Executed, FGameplayCueParameters());
}

const UNGControlStateDefinition* UNGCharacterStateMachineComponent::GetAbilityStateDefinition(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const
{
	return OwnsStateAbility(ASC, Handle) ? ActiveResources->GetDefinition() : nullptr;
}

UAbilitySystemComponent* UNGCharacterStateMachineComponent::GetReadyASC() const
{
	auto* ASC = ReadyASC.Get();
	return ASC && ASC->GetAvatarActor() == GetOwner() ? ASC : nullptr;
}

bool UNGCharacterStateMachineComponent::OwnsStateAbility(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const
{
	return ActiveResources && ActiveResources->GetDefinition()->StateTag == ControlState.Tag && ActiveResources->OwnsAbility(ASC, Handle);
}

bool UNGCharacterStateMachineComponent::IsGameplayReady(FGameplayTag RequiredState) const
{
	const auto* ASC = GetReadyASC();
	if (bEnding || HasResourceFailure() || !ASC || !ActiveResources || !ActiveResources->IsEntryReady()
		|| (RequiredState.IsValid() && !HasStateTag(RequiredState))) return false;
	const auto* Definition = ActiveResources->GetDefinition();
	if (Definition->StateTag != ControlState.Tag) return false;

	for (const auto& Grant : Definition->Abilities)
	{
		bool bFound = false;
		for (const auto& Spec : ASC->GetActivatableAbilities())
			if (Spec.Ability && Spec.Ability->GetClass() == Grant.Ability.Get()) { bFound = true; break; }
		if (!bFound) return false;
	}
	return true;
}

void UNGCharacterStateMachineComponent::RefreshResources()
{
	if (bEnding || HasResourceFailure() || bApplyingState || !HasBegunPlay()) return;
	auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;

	TGuardValue<bool> ApplyingState(bApplyingState, true);
	const bool bAuthority = Pawn->HasAuthority();
	if (bAuthority && !ControlState.Tag.IsValid() && Pawn->HasActorBegunPlay() && GetReadyASC()) SetStateTag(TAG_NGAlive);

	FGameplayTag ApplyingTag;
	do
	{
		ApplyingTag = ControlState.Tag;
		if (ActiveResources && ActiveResources->GetDefinition()->StateTag != ApplyingTag) ReleaseActiveResources();
		NotifyStateChanged();
		if (ApplyingTag != ControlState.Tag) continue;
		if (bEnding || HasResourceFailure() || !ApplyingTag.IsValid()) return;
		if (bAuthority && (!Pawn->HasActorBegunPlay() || !GetReadyASC())) return;

		if (ActiveResources) ActiveResources->RefreshResources();
		else if (auto* Definition = FindDefinition(ApplyingTag))
		{
			ActiveResources = NewObject<UNGCharacterStateResources>(this, NAME_None, RF_Transient);
			ActiveResources->Enter(Pawn, Definition);
		}
		else NotifyResourceFailure(TEXT("Assign exactly one matching asset in State Definitions."));
	} while (!bEnding && !HasResourceFailure() && ApplyingTag != ControlState.Tag);
}

void UNGCharacterStateMachineComponent::ReleaseActiveResources()
{
	auto* Previous = ActiveResources.Get();
	ActiveResources = nullptr;
	if (Previous) Previous->Exit();
}

void UNGCharacterStateMachineComponent::SuspendResources()
{
	UnbindAbilitySpecNotifications();
	ReadyASC.Reset();
	ReleaseActiveResources();
}

void UNGCharacterStateMachineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, ControlState);
}

void UNGCharacterStateMachineComponent::EndPlay(EEndPlayReason::Type Reason)
{
	bEnding = true;
	SuspendResources();
	Super::EndPlay(Reason);
}
