#pragma once

#include "Gameplay/AI/NGAIActionState.h"
#include "AITypes.h"
#include "NGAIMoveState.generated.h"

class UPathFollowingComponent;

UCLASS(Blueprintable)
class NEXTGENONE_API UNGAIMoveState : public UNGAIActionState
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true")) FVector Destination = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true")) bool bRelativeToPawn = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true", ClampMin="0")) float AcceptanceRadius = 50.f;
protected:
	virtual void StartAction() override;
	virtual void ReleaseActionResources() override;
private:
	TWeakObjectPtr<UPathFollowingComponent> PathFollowing;
	FAIRequestID MoveRequestID = FAIRequestID::InvalidRequest;
	FDelegateHandle MoveFinishedHandle;
};
