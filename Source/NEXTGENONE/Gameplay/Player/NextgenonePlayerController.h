#pragma once
#include "CoreMinimal.h"
#include "Character/ALSPlayerController.h"
#include "NextgenonePlayerController.generated.h"

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
private:
	TWeakObjectPtr<APawn> PreviousInputPawn;
    void RefreshPawnPresentation();
};
