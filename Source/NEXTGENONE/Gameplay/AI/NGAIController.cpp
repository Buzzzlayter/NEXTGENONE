#include "Gameplay/AI/NGAIController.h"

#include "GameFramework/Pawn.h"

ANGAIController::ANGAIController()
{
	// Character initialization requires the GameMode's ANextgenonePlayerState (ASC + inventory).
	bWantsPlayerState = true;
	// A future brain must wait for the current Avatar and Character FSM resources.
	bStartAILogicOnPossess = false;
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
