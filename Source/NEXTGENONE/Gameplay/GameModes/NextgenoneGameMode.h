#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NextgenoneGameMode.generated.h"

class AHUD;

/** One gameplay configuration shared by the game and its combat test mode. */
UCLASS()
class NEXTGENONE_API ANextgenoneGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ANextgenoneGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Classes")
	TSoftClassPtr<APawn> GameplayCharacterClass;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Classes")
	TSoftClassPtr<APlayerController> GameplayControllerClass;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Classes")
	TSoftClassPtr<AHUD> GameplayHUDClass;
};
