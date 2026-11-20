#include "Gameplay/AI/NGBotCharacter.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Teams/NGTeamComponent.h"

void ANGBotCharacter::PossessedBy(AController* NewController)
{
	if (NewController && !NewController->IsPlayerController())
	{
		auto* Team = UNGTeamComponent::Find(NewController->GetPlayerState<APlayerState>());
		if (Team && Team->GetTeamName().IsNone())
			Team->SetTeamName(InitialTeamName);
	}
	Super::PossessedBy(NewController);
}
