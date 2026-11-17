#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NGAimComponent.generated.h"

UCLASS(ClassGroup=Gameplay,meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGAimComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGAimComponent();
	FSimpleMulticastDelegate OnAimUpdated;
	UFUNCTION(BlueprintPure,Category="Aim") bool GetViewRay(FVector& Origin,FVector& Direction) const;
	/** Server bounds for the owning client's shoulder-camera ray. */
	UPROPERTY(EditDefaultsOnly, Category="Aim|Validation", meta=(ClampMin="1")) float MaxViewOriginDistance=600.f;
	UPROPERTY(EditDefaultsOnly, Category="Aim|Validation", meta=(ClampMin="0",ClampMax="90")) float MaxViewAngleDifference=35.f;
	bool ValidateShotView(const FVector& Origin, const FVector& Direction) const;
	virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* ThisTick) override;
};
