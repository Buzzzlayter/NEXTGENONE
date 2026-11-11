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
	uint8 GetDefaultTeamId(const AController& Controller) const;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Teams", meta=(ClampMax="254")) uint8 PlayerTeamId = 0;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Teams", meta=(ClampMax="254")) uint8 BotTeamId = 1;

	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Classes")
	TSoftClassPtr<APawn> GameplayCharacterClass;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Classes")
	TSoftClassPtr<APlayerController> GameplayControllerClass;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Classes")
	TSoftClassPtr<AHUD> GameplayHUDClass;
};
