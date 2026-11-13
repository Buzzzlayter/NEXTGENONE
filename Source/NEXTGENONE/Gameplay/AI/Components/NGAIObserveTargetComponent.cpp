#include "Gameplay/AI/Components/NGAIObserveTargetComponent.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"
#include "GameFramework/Pawn.h"

void UNGAIObserveTargetComponent::Enter()
{
	StopObservation();
	Controller = Cast<ANGAIController>(GetContextOwner());
	auto* AI = Controller.Get();
	ExpectedPawn = AI ? AI->GetPawn() : nullptr;
	if (AI && ExpectedPawn.IsValid() && AI->GetReadyAbilitySystem(ExpectedPawn.Get()))
	{
		Memory = AI->GetMemory();
		if (auto* TargetMemory = Memory.Get())
		{
			MemoryUpdatedHandle = TargetMemory->OnMemoryUpdated.AddUObject(this, &ThisClass::UpdateObservation);
			UpdateObservation();
		}
	}
	Super::Enter();
}

void UNGAIObserveTargetComponent::UpdateObservation()
{
	auto* AI = Controller.Get();
	if (!AI || !ExpectedPawn.IsValid() || AI->GetPawn() != ExpectedPawn.Get()
		|| !AI->GetReadyAbilitySystem(ExpectedPawn.Get()) || !Memory.IsValid() || !Memory->IsMemoryEnabled())
	{
		StopObservation();
		return;
	}
	const auto* Target = Memory.IsValid() ? Memory->GetTarget() : nullptr;
	if (!Target || (!Target->bVisible && !bObserveLastKnownLocation))
	{
		ReleaseFocus();
		return;
	}
	FocusActor = Target->bVisible ? Target->Actor : TWeakObjectPtr<AActor>();
	FocusPoint = Target->GetKnownLocation();
	if (FocusActor.IsValid()) AI->SetFocus(FocusActor.Get(), EAIFocusPriority::Gameplay);
	else AI->SetFocalPoint(FocusPoint, EAIFocusPriority::Gameplay);
	bOwnsFocus = true;
}

void UNGAIObserveTargetComponent::ReleaseFocus()
{
	if (auto* AI = Controller.Get(); AI && bOwnsFocus)
	{
		// Never clear FireBurst's higher-priority focus or a different replacement in our slot.
		const auto* CurrentActor = AI->GetFocusActorForPriority(EAIFocusPriority::Gameplay);
		if ((FocusActor.IsValid() && CurrentActor == FocusActor.Get())
			|| (!FocusActor.IsValid() && !CurrentActor && AI->GetFocalPointForPriority(EAIFocusPriority::Gameplay).Equals(FocusPoint)))
			AI->ClearFocus(EAIFocusPriority::Gameplay);
	}
	bOwnsFocus = false;
	FocusActor.Reset();
}

void UNGAIObserveTargetComponent::StopObservation()
{
	if (auto* TargetMemory = Memory.Get()) TargetMemory->OnMemoryUpdated.Remove(MemoryUpdatedHandle);
	MemoryUpdatedHandle.Reset();
	ReleaseFocus();
	Memory.Reset();
	ExpectedPawn.Reset();
	Controller.Reset();
}

void UNGAIObserveTargetComponent::Exit(FName Event)
{
	StopObservation();
	Super::Exit(Event);
}

void UNGAIObserveTargetComponent::BeginDestroy()
{
	StopObservation();
	Super::BeginDestroy();
}
