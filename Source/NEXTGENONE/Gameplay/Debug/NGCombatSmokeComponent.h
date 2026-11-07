#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NGCombatSmokeComponent.generated.h"

class ANextgenoneCharacter;

USTRUCT()
struct FNGCombatSmokeRequest
{
	GENERATED_BODY()
	UPROPERTY() int32 Command = 0;
	UPROPERTY() TObjectPtr<ANextgenoneCharacter> Shooter;
	UPROPERTY() TObjectPtr<ANextgenoneCharacter> Target;
	UPROPERTY() float ExpectedHealth = 0.f;
	UPROPERTY() int32 ExpectedAmmo = INDEX_NONE;
	UPROPERTY() int32 ActionSlot = 0;
};

/** Opt-in test transport, created only by the combat test GameMode. */
UCLASS()
class NEXTGENONE_API UNGCombatSmokeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGCombatSmokeComponent();
	UFUNCTION(Client, Reliable) void ClientSmoke(FNGCombatSmokeRequest Request);
private:
	UFUNCTION(Server, Reliable) void ServerSmokeResult(int32 Command, bool bPassed);
};
