#pragma once
#include "CoreMinimal.h"
#include "Gameplay/GameModes/NextgenoneGameMode.h"
#include "NGTestGameMode.generated.h"

class ANextgenonePlayerController;
class ANextgenoneCharacter;
UCLASS()
class NEXTGENONE_API ANGCombatTestGameMode : public ANextgenoneGameMode
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	void RecordSmoke(ANextgenonePlayerController* PC, int32 Command, bool bPassed);
private:
	FTimerHandle SmokeTimer;
	TArray<TWeakObjectPtr<ANextgenonePlayerController>> Players;
	TSet<TWeakObjectPtr<ANextgenonePlayerController>> Reports;
	int32 Phase = 0;
	int32 ExpectedReport = -1;
	int32 Shots = 0;
	int32 ActionSlot = INDEX_NONE;
	int32 ExpectedAmmo = 0;
	float ExpectedHealth = 0.f;
	float ShotDamage = 0.f;
	float ShotWait = 0.f;
	TWeakObjectPtr<ANextgenoneCharacter> Corpse;
	double PhaseTime = 0;
	double Deadline = 0;
	void SmokeTick();
	void Fail(const FString& Reason);
	void AskClients(int32 Command);
	void SendShooter(int32 Command);
};
