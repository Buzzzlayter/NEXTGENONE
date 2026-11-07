#pragma once
#include "CoreMinimal.h"
#include "Character/ALSPlayerController.h"
#include "NextgenonePlayerController.generated.h"

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

UCLASS()
class NEXTGENONE_API ANextgenonePlayerController : public AALSPlayerController
{
    GENERATED_BODY()
public:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void AddYawInput(float Value) override;
    virtual void AddPitchInput(float Value) override;
    virtual void OnPossess(APawn* ControlledPawn) override;
    virtual void OnRep_Pawn() override;
    // Existing test transport remains opt-in; no input bindings or tests are installed here.
    UFUNCTION(Client, Reliable) void ClientSmoke(FNGCombatSmokeRequest Request);
    UFUNCTION(Server, Reliable) void ServerSmokeResult(int32 Command, bool bPassed);
private:
	TWeakObjectPtr<APawn> PreviousInputPawn;
    void RefreshPawnPresentation();
};
