#pragma once

#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "NGAIMemoryComponent.generated.h"

/** Knowledge, not an Actor-position cache: hidden actors never update these locations. */
struct FNGTargetMemory
{
	TWeakObjectPtr<AActor> Actor;
	FVector LastSeenLocation = FVector::ZeroVector;
	FVector LastHeardLocation = FVector::ZeroVector;
	double LastSeenTime = -1.;
	double LastHeardTime = -1.;
	bool bVisible = false;
	FVector GetKnownLocation() const { return LastSeenTime >= LastHeardTime ? LastSeenLocation : LastHeardLocation; }
	double GetLastObservationTime() const { return FMath::Max(LastSeenTime, LastHeardTime); }
};

/** Server-only, bounded working memory and a stable selection of hostile targets. */
UCLASS()
class NEXTGENONE_API UNGAIMemoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="AI|Memory", meta=(ClampMin="0.1")) float RetentionSeconds = 8.f;
	UPROPERTY(EditDefaultsOnly, Category="AI|Memory", meta=(ClampMin="0.05")) float UpdateInterval = .2f;
	UPROPERTY(EditDefaultsOnly, Category="AI|Memory", meta=(ClampMin="1", ClampMax="256")) int32 MaxRememberedActors = 32;
	/** A challenger must be this much closer before replacing an equally visible target. */
	UPROPERTY(EditDefaultsOnly, Category="AI|Memory", meta=(ClampMin="0")) float TargetSwitchDistance = 300.f;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	void SetEnabled(bool bEnable);
	const FNGTargetMemory* GetTarget() const;
private:
	TMap<TWeakObjectPtr<AActor>, FNGTargetMemory> Observations;
	TWeakObjectPtr<AActor> SelectedTarget;
	FTimerHandle UpdateTimer;
	bool bEnabled = false;
	UFUNCTION() void PerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	void Refresh();
	bool IsCandidate(AActor* Actor) const;
};
