#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UIMarkerSubsystem.generated.h"

class AUIMarkerReplicator;
class AActor;
class APlayerController;
class ULocalPlayer;
class UUIMarkerModel;
class UUIMarkerObserver;

UCLASS()
class UIMODULE_API UUIMarkerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	bool RegisterMarker(UUIMarkerModel* Model);
	bool PlaceMarkerForSource(AActor* Source, UUIMarkerModel* MarkerTemplate, const FVector& Position, FName RecipientGroup);
	void UnregisterMarker(UUIMarkerModel* Model);
	UUIMarkerObserver* GetMarkerObserver(ULocalPlayer* Player) const;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void BindMarkerReplicator(AActor* Actor);
	void AddMarker(UUIMarkerModel* Model);
	void RemoveMarker(UUIMarkerModel* Model);
	UFUNCTION()
	void OnMarkerSourceEndPlay(AActor* Source, EEndPlayReason::Type EndPlayReason);
	void AddLocalObserver(ULocalPlayer* Player);
	void RemoveLocalObserver(ULocalPlayer* Player);
	void LocalControllerChanged(APlayerController* Controller, ULocalPlayer* Player);

	UPROPERTY(Transient)
	TSet<TObjectPtr<UUIMarkerModel>> Markers;
	UPROPERTY(Transient)
	TMap<TObjectPtr<ULocalPlayer>, TObjectPtr<UUIMarkerObserver>> MapObservers;

	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UUIMarkerModel>> MarkersBySource;
	TWeakObjectPtr<AUIMarkerReplicator> Replicator;
	FDelegateHandle ActorPreSpawnHandle;
};
