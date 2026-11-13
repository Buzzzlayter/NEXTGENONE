#pragma once

#include "Gameplay/AI/States/NGAIActionState.h"
#include "AITypes.h"
#include "NGAIMoveState.generated.h"

class UPathFollowingComponent;

UENUM(BlueprintType)
enum class ENGAIMoveDestination : uint8
{
	Point,
	LastKnownTarget,
	Reposition
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGAIMoveState : public UNGAIActionState
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true")) ENGAIMoveDestination DestinationType = ENGAIMoveDestination::Point;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true", EditCondition="DestinationType == ENGAIMoveDestination::Point")) FVector Destination = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true", EditCondition="DestinationType == ENGAIMoveDestination::Point")) bool bRelativeToPawn = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true", ClampMin="0")) float AcceptanceRadius = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true", ClampMin="100", EditCondition="DestinationType == ENGAIMoveDestination::Reposition")) float MinTargetDistance = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true", ClampMin="100", EditCondition="DestinationType == ENGAIMoveDestination::Reposition")) float MaxTargetDistance = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Move", meta=(ExposeOnSpawn="true", ClampMin="0", EditCondition="DestinationType == ENGAIMoveDestination::Reposition")) float MinMoveDistance = 200.f;
protected:
	virtual void StartAction() override;
	virtual void ReleaseActionResources() override;
private:
	bool ResolveDestination(FVector& Goal) const;
	TWeakObjectPtr<UPathFollowingComponent> PathFollowing;
	FAIRequestID MoveRequestID = FAIRequestID::InvalidRequest;
	FDelegateHandle MoveFinishedHandle;
};
