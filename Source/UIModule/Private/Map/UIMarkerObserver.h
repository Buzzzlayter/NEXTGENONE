#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "UObject/Object.h"
#include "UIMarkerObserver.generated.h"

class ULocalPlayer;
class APlayerController;
class UWorld;
class UUIMarkerModel;
struct FUIMarkerVisibilityRules;
enum class EUIMarkerSurface : uint8;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FUIMarkerVisible, UUIMarkerModel*, EUIMarkerSurface, bool);
DECLARE_MULTICAST_DELEGATE_TwoParams(FUIMarkerHidden, FGuid, EUIMarkerSurface);

UCLASS()
class UUIMarkerObserver : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UWorld& InWorld, ULocalPlayer& InViewer);
	void AddMarker(UUIMarkerModel* Model);
	void RemoveMarker(TWeakObjectPtr<UUIMarkerModel> Marker);
	void Reset();
	void RefreshVisibility(bool bIncludeStatic = false);
	TArray<UUIMarkerModel*> GetVisibleMarkers(EUIMarkerSurface Surface) const;
	FUIMarkerVisible OnMarkerVisible;
	FUIMarkerHidden OnMarkerHidden;

private:
	void EvaluateMarker(UUIMarkerModel* Model, bool bPresentationChanged, bool bDataChanged);
	bool CanSee(const APlayerController& Player, const FVector& Position, const FUIMarkerVisibilityRules& Visibility, TOptional<bool>& LineOfSight) const;

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ULocalPlayer> Viewer;
	FTimerHandle VisibilityTimer;
	struct FVisibleMarker
	{
		FGuid VisibleId;
		FVector VisiblePosition = FVector::ZeroVector;
	};

	using FSurfaceStates = TMap<EUIMarkerSurface, FVisibleMarker>;
	TMap<TWeakObjectPtr<UUIMarkerModel>, FSurfaceStates> ObservedMarkers;
};
