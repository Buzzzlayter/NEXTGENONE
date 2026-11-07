#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NGAimComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNGAimUpdated,AActor*,Target,FVector,AimPoint,bool,bBlockingHit);

/** Local view/reticle logic. Damage and interaction authority remain in their existing systems. */
UCLASS(ClassGroup=Gameplay,meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGAimComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGAimComponent();
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Aim") bool bShowReticle=true;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Aim",meta=(ClampMin="100",ClampMax="100000")) float TraceDistance=10000.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Aim",meta=(ClampMin="2",ClampMax="16")) float ReticleSize=5.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Aim") FLinearColor IdleColor=FLinearColor::White;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Aim") FLinearColor InteractionColor=FLinearColor(.2f,1.f,.4f);
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Aim") FLinearColor UnavailableColor=FLinearColor(1.f,.65f,.15f);
	UPROPERTY(BlueprintAssignable,Category="Aim") FNGAimUpdated OnAimUpdated;
	UFUNCTION(BlueprintPure,Category="Aim") bool GetViewRay(FVector& Origin,FVector& Direction) const;
	/** Server bounds for the owning client's shoulder-camera ray. */
	UPROPERTY(EditDefaultsOnly, Category="Aim|Validation", meta=(ClampMin="1")) float MaxViewOriginDistance=600.f;
	UPROPERTY(EditDefaultsOnly, Category="Aim|Validation", meta=(ClampMin="0",ClampMax="90")) float MaxViewAngleDifference=35.f;
	bool ValidateShotView(const FVector& Origin, const FVector& Direction) const;
	FVector GetTargetingOrigin(const FVector& ViewOrigin, const FVector& ViewDirection) const;
	UFUNCTION(BlueprintPure,Category="Aim") bool ShouldShowReticle() const;
	UFUNCTION(BlueprintPure,Category="Aim") FLinearColor GetReticleColor() const;
	UFUNCTION(BlueprintPure,Category="Aim") FVector GetAimPoint() const { return AimPoint; }
	UFUNCTION(BlueprintPure,Category="Aim") AActor* GetAimTarget() const { return AimTarget.Get(); }
	UFUNCTION(BlueprintPure,Category="Aim") bool HasBlockingHit() const { return bHit; }
	virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* ThisTick) override;
private:
	FVector AimPoint=FVector::ZeroVector;
	TWeakObjectPtr<AActor> AimTarget;
	bool bHit=false;
};
