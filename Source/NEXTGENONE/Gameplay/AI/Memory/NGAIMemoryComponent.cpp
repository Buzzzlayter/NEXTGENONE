#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/Teams/NGTeamComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AIPerceptionTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UNGAIMemoryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* AI = Cast<ANGAIController>(GetOwner()); AI && AI->HasAuthority() && AI->GetPerceptionComponent())
		AI->GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &ThisClass::PerceptionUpdated);
}

void UNGAIMemoryComponent::EndPlay(EEndPlayReason::Type Reason)
{
	SetEnabled(false);
	if (auto* AI = Cast<ANGAIController>(GetOwner()); AI && AI->GetPerceptionComponent())
		AI->GetPerceptionComponent()->OnTargetPerceptionUpdated.RemoveDynamic(this, &ThisClass::PerceptionUpdated);
	Super::EndPlay(Reason);
}

bool UNGAIMemoryComponent::IsCandidate(AActor* Actor) const
{
	const auto* AI = Cast<ANGAIController>(GetOwner());
	const auto* Pawn = Cast<APawn>(Actor);
	const auto* Team = UNGTeamComponent::Find(AI);
	if (!AI || !AI->HasAuthority() || !IsValid(Pawn) || Pawn == AI->GetPawn()
		|| !Team || Team->GetAttitudeTowards(Pawn) != ETeamAttitude::Hostile) return false;
	const auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	return ASC && ASC->GetAvatarActor() == Pawn && !ASC->HasMatchingGameplayTag(TAG_NGDead);
}

void UNGAIMemoryComponent::SetEnabled(bool bEnable)
{
	if (bEnable == bEnabled) return;
	bEnabled = bEnable && GetOwner()->HasAuthority();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	Observations.Empty();
	SelectedTarget.Reset();
	if (auto* AI = Cast<ANGAIController>(GetOwner()))
	{
		AI->RefreshObservation();
		if (!bEnabled && AI->GetPerceptionComponent()) AI->GetPerceptionComponent()->ForgetAll();
	}
	if (bEnabled && GetWorld())
	{
		Refresh(); // Seed from already-perceived actors: possession can precede gameplay readiness.
		GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::Refresh,
			FMath::IsFinite(UpdateInterval) ? FMath::Max(.05f, UpdateInterval) : .2f, true);
	}
}

void UNGAIMemoryComponent::PerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!bEnabled || !IsCandidate(Actor)) return;
	const bool bSight = Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>();
	const bool bHearing = Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>();
	if (!bSight && !bHearing) return;
	auto* Record = Observations.Find(Actor);
	if (!Record)
	{
		if (!Stimulus.WasSuccessfullySensed() || Observations.Num() >= FMath::Clamp(MaxRememberedActors, 1, 256)) return;
		Record = &Observations.Add(Actor);
		Record->Actor = Actor;
	}
	if (bSight) Record->bVisible = Stimulus.WasSuccessfullySensed();
	if (Stimulus.WasSuccessfullySensed() && !Stimulus.StimulusLocation.ContainsNaN())
	{
		if (bSight) { Record->LastSeenLocation = Stimulus.StimulusLocation; Record->LastSeenTime = GetWorld()->GetTimeSeconds(); }
		else { Record->LastHeardLocation = Stimulus.StimulusLocation; Record->LastHeardTime = GetWorld()->GetTimeSeconds(); }
	}
	Refresh();
}

void UNGAIMemoryComponent::Refresh()
{
	if (!bEnabled) return;
	auto* AI = Cast<ANGAIController>(GetOwner());
	APawn* ControlledPawn = AI ? AI->GetPawn() : nullptr;
	if (!ControlledPawn) { SetEnabled(false); return; }
	TArray<AActor*> Visible;
	AI->GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Visible);
	for (auto& Pair : Observations) Pair.Value.bVisible = false;
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto* Actor : Visible)
	{
		if (!IsCandidate(Actor)) continue;
		auto* Record = Observations.Find(Actor);
		if (!Record)
		{
			if (Observations.Num() >= FMath::Clamp(MaxRememberedActors, 1, 256)) continue;
			Record = &Observations.Add(Actor); Record->Actor = Actor;
		}
		// Only read a live transform after the sense confirms current visibility.
		Record->LastSeenLocation = CastChecked<APawn>(Actor)->GetPawnViewLocation();
		Record->LastSeenTime = Now;
		Record->bVisible = true;
	}
	const float Lifetime = FMath::IsFinite(RetentionSeconds) ? FMath::Max(.1f, RetentionSeconds) : 8.f;
	for (auto It = Observations.CreateIterator(); It; ++It)
		if (!IsCandidate(It.Key().Get()) || (!It.Value().bVisible && Now - It.Value().GetLastObservationTime() > Lifetime))
			It.RemoveCurrent();
	const auto* Current = Observations.Find(SelectedTarget);
	const FNGTargetMemory* Best = Current;
	float BestDistance = Best ? FVector::Dist(ControlledPawn->GetActorLocation(), Best->GetKnownLocation()) : MAX_flt;
	for (const auto& Pair : Observations)
	{
		const auto& Candidate = Pair.Value;
		const float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), Candidate.GetKnownLocation());
		const float Margin = FMath::IsFinite(TargetSwitchDistance) ? FMath::Max(0.f, TargetSwitchDistance) : 300.f;
		if (!Best || (Candidate.bVisible && !Best->bVisible)
			|| (Candidate.bVisible == Best->bVisible && Distance + Margin < BestDistance))
		{ Best = &Candidate; BestDistance = Distance; }
	}
	SelectedTarget = Best ? Best->Actor : TWeakObjectPtr<AActor>();
	AI->RefreshObservation();
}

const FNGTargetMemory* UNGAIMemoryComponent::GetTarget() const
{
	const auto* Record = Observations.Find(SelectedTarget);
	return bEnabled && Record && IsCandidate(Record->Actor.Get()) ? Record : nullptr;
}
