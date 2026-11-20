#pragma once

#include "Gameplay/Character/NextgenoneCharacter.h"
#include "NGBotCharacter.generated.h"

class UNGAIBehavior;

/** Shared character gameplay with bot-specific authoring settings. */
UCLASS(PrioritizeCategories = ("Team"))
class NEXTGENONE_API ANGBotCharacter : public ANextgenoneCharacter
{
	GENERATED_BODY()
public:
	virtual void PossessedBy(AController* NewController) override;

	/** None uses the AI controller's default behavior. */
	UPROPERTY(EditAnywhere, Category="AI", meta=(DisplayName="AI Behavior Class")) TSubclassOf<UNGAIBehavior> AIBehaviorClass;

private:
	UPROPERTY(EditAnywhere, Category = "Team")
	FName InitialTeamName = TEXT("Bots");
};
