#include "Gameplay/AI/NGAIController.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "GameFramework/Pawn.h"

ANGAIController::ANGAIController()
{
	// Character initialization requires the GameMode's ANextgenonePlayerState (ASC + inventory).
	bWantsPlayerState = true;
	// A future brain must wait for the current Avatar and Character FSM resources.
	bStartAILogicOnPossess = false;
}

UNGAbilitySystemComponent* ANGAIController::GetReadyAbilitySystem(const APawn* ExpectedPawn) const
{
	const auto* Character = Cast<ANextgenoneCharacter>(GetPawn());
	if (!HasAuthority() || !IsValid(Character) || Character != ExpectedPawn || Character->GetController() != this
		|| Character->IsDead() || !Character->CharacterStateMachine
		|| !Character->CharacterStateMachine->IsGameplayReady(TAG_NGAlive)) return nullptr;
	auto* ASC = Cast<UNGAbilitySystemComponent>(Character->GetAbilitySystemComponent());
	return ASC && ASC->GetAvatarActor() == Character ? ASC : nullptr;
}

FVector ANGAIController::GetFocalPointOnActor(const AActor* Actor) const
{
	// Preserve ALS focus at the Pawn's view location without its Behavior Tree startup.
	if (const APawn* FocusPawn = Cast<APawn>(Actor))
	{
		return FocusPawn->GetPawnViewLocation();
	}
	return Super::GetFocalPointOnActor(Actor);
}
