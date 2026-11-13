#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/TimerHandle.h"
#include "NextgenoneGameMode.generated.h"

class AHUD;
class APlayerController;

/** One gameplay configuration shared by the game and its combat test mode. */
UCLASS()
class NEXTGENONE_API ANextgenoneGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ANextgenoneGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Respawn", meta=(ClampMin="0.1")) float RespawnDelay = 3.f;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Respawn", meta=(ClampMin="1")) float CorpseLifetime = 15.f;
	uint8 GetDefaultTeamId(const AController& Controller) const;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Teams", meta=(ClampMax="254")) uint8 PlayerTeamId = 0;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Teams", meta=(ClampMax="254")) uint8 BotTeamId = 1;

	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Classes")
	TSoftClassPtr<APawn> GameplayCharacterClass;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Classes")
	TSoftClassPtr<APlayerController> GameplayControllerClass;
	UPROPERTY(EditDefaultsOnly, Category="NEXTGENONE|Classes")
	TSoftClassPtr<AHUD> GameplayHUDClass;
private:
	void ScheduleRespawn(APawn* DeadPawn);
	void ClearRespawn(AController* Controller);
	TMap<TWeakObjectPtr<APlayerController>, FTimerHandle> RespawnTimers;
};
