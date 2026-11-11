#include "Gameplay/AI/States/NGAIPauseState.h"

#include "Gameplay/AI/NGAIBehavior.h"
#include "Gameplay/AI/NGAIEvents.h"

TArray<FName> UNGAIPauseState::GetDeclaredTransitionEvents() const
{
	return {NGAIEvents::Succeeded};
}

void UNGAIPauseState::Enter()
{
	RemainingTime = FMath::IsFinite(Cooldown) ? FMath::Max(0.f, Cooldown) : 0.f;
	bCompleted = false;
	if (!FMath::IsFinite(Cooldown) || Cooldown < 0.f)
		UE_LOG(LogTemp, Warning, TEXT("%s: invalid pause Cooldown; using zero."), *GetName());
	Super::Enter();
}

void UNGAIPauseState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bCompleted || !Context || !GetParentFSM() || GetParentFSM()->GetActiveState() != this) return;
	RemainingTime -= DeltaTime;
	if (RemainingTime > 0.f) return;
	bCompleted = true;
	TriggerTransitionEvent(NGAIEvents::Succeeded);
}
