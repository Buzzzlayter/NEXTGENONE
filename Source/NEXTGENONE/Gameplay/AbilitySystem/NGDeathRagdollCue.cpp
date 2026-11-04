#include "Gameplay/AbilitySystem/NGDeathRagdollCue.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Character/ALSBaseCharacter.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGDeathRagdollCue, "GameplayCue.Character.Death.Ragdoll");

UNGDeathRagdollCue::UNGDeathRagdollCue(const FObjectInitializer& Init) : Super(Init)
{
	GameplayCueTag = TAG_NGDeathRagdollCue;
}

bool UNGDeathRagdollCue::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	auto* Pawn = Cast<AALSBaseCharacter>(Target);
	auto* Machine = Pawn ? Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>() : nullptr;
	if (Parameters.SourceObject.IsValid() && Parameters.SourceObject.Get() != Target) return false;
	// A late cue from the previous ASC Avatar must not affect a new life.
	// FSM entry/BeginPlay restores presentation if this cue arrives before the state.
	if (!Pawn || !Pawn->HasActorBegunPlay() || !Machine || !Machine->HasStateTag(TAG_NGDead)) return false;
	if (Pawn->GetMovementState() != EALSMovementState::Ragdoll) Pawn->RagdollStart();
	return true;
}
