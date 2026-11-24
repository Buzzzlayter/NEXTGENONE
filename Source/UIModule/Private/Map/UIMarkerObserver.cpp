#include "Map/UIMarkerObserver.h"

#include "Algo/NoneOf.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Map/UIMarkerModel.h"
#include "Map/UIMarkerWidget.h"
#include "TimerManager.h"

void UUIMarkerObserver::Initialize(UWorld& InWorld, ULocalPlayer& InViewer)
{
	World = &InWorld;
	Viewer = &InViewer;
	InWorld.GetTimerManager().SetTimer(VisibilityTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::RefreshVisibility, false), 0.2f, true);
}

TArray<UUIMarkerModel*> UUIMarkerObserver::GetVisibleMarkers(EUIMarkerSurface Surface) const
{
	TArray<UUIMarkerModel*> Models;
	for (const auto& Pair : ObservedMarkers)
		if (auto* Model = Pair.Key.Get(); Model && Pair.Value.Contains(Surface)) Models.Add(Model);
	return Models;
}

void UUIMarkerObserver::RefreshVisibility(bool bIncludeStatic)
{
	TArray<TWeakObjectPtr<UUIMarkerModel>, TInlineAllocator<32>> Models;
	ObservedMarkers.GenerateKeyArray(Models);
	for (const auto& Marker : Models)
	{
		if (auto* Model = Marker.Get()) EvaluateMarker(Model, bIncludeStatic, bIncludeStatic);
		else RemoveMarker(Marker);
	}
}

void UUIMarkerObserver::AddMarker(UUIMarkerModel* Model)
{
	if (!IsValid(Model) || ObservedMarkers.Contains(Model)) return;
	ObservedMarkers.Add(Model);
	Model->OnMarkerChanged().AddUObject(this, &ThisClass::EvaluateMarker, true);
	EvaluateMarker(Model, true, true);
}

void UUIMarkerObserver::RemoveMarker(TWeakObjectPtr<UUIMarkerModel> Marker)
{
	FSurfaceStates States;
	if (!ObservedMarkers.RemoveAndCopyValue(Marker, States)) return;
	if (auto* Model = Marker.Get()) Model->OnMarkerChanged().RemoveAll(this);
	for (const auto& Pair : States) OnMarkerHidden.Broadcast(Pair.Value.VisibleId, Pair.Key);
}

void UUIMarkerObserver::Reset()
{
	if (auto* MarkerWorld = World.Get()) MarkerWorld->GetTimerManager().ClearTimer(VisibilityTimer);
	TArray<TWeakObjectPtr<UUIMarkerModel>> Models;
	ObservedMarkers.GenerateKeyArray(Models);
	for (const auto& Marker : Models) RemoveMarker(Marker);
}

void UUIMarkerObserver::EvaluateMarker(UUIMarkerModel* Model, bool bPresentationChanged, bool bDataChanged)
{
	const bool bHasRecipientGroup = !Model->GetRecipientGroup().IsNone();
	if (!bDataChanged && !bHasRecipientGroup && Algo::NoneOf(TEnumRange<EUIMarkerSurface>(), [Model](EUIMarkerSurface Surface)
	{
		const auto* Visibility = Model->GetVisibility(Surface);
		return Visibility && Visibility->RequiresViewer();
	}))
		return;

	auto* LocalPlayer = Viewer.Get();
	const auto* MarkerWorld = World.Get();
	const auto* Player = MarkerWorld && LocalPlayer ? LocalPlayer->UPlayer::GetPlayerController(MarkerWorld) : nullptr;
	const bool bCanViewMarker = Player && Model->IsVisibleTo(*Player);
	const FGuid Id = Model->GetMarkerId();
	const FVector Position = Model->GetPosition();
	TOptional<bool> LineOfSight;
	for (auto Surface : TEnumRange<EUIMarkerSurface>())
	{
		auto* States = ObservedMarkers.Find(Model);
		if (!States) return;
		const auto* Visibility = Model->GetVisibility(Surface);
		if (!bDataChanged && (!Visibility || (!bHasRecipientGroup && !Visibility->RequiresViewer()))) continue;

		const auto* WidgetDefaults = Model->GetWidgetClass(Surface)->GetDefaultObject<UUIMarkerWidget>();
		const bool bOwnWorldMarker = Player && Surface == EUIMarkerSurface::World && Model->GetSourceActor()
			&& Model->GetSourceActor() == Player->GetPawn().Get() && WidgetDefaults->GetOwnMarkerScreenAnchor().IsSet();
		if (!Visibility || !Id.IsValid() || !bCanViewMarker || !WidgetDefaults->SupportsSurface(Surface)
			|| (!bOwnWorldMarker && !CanSee(*Player, Position, *Visibility, LineOfSight)))
		{
			FVisibleMarker Previous;
			if (States->RemoveAndCopyValue(Surface, Previous)) OnMarkerHidden.Broadcast(Previous.VisibleId, Surface);
			continue;
		}

		auto& State = States->FindOrAdd(Surface);
		if (!bDataChanged && State.VisibleId == Id && State.VisiblePosition == Position) continue;
		State.VisibleId = Id;
		State.VisiblePosition = Position;
		OnMarkerVisible.Broadcast(Model, Surface, bPresentationChanged);
	}
}

bool UUIMarkerObserver::CanSee(const APlayerController& Player, const FVector& Position, const FUIMarkerVisibilityRules& Visibility, TOptional<bool>& LineOfSight) const
{
	if (!Visibility.RequiresViewer()) return true;

	const APawn* Pawn = Player.GetPawn().Get();
	if (Visibility.VisibilityRadius > 0.f && (!Pawn || FVector::DistSquared(Position, Pawn->GetActorLocation()) > FMath::Square(Visibility.VisibilityRadius)))
		return false;
	if (!Visibility.bRequireLineOfSight) return true;
	if (!LineOfSight.IsSet())
	{
		FVector CameraPosition;
		FRotator CameraRotation;
		Player.GetPlayerViewPoint(CameraPosition, CameraRotation);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(UIMarkerVisibility), false, Pawn);
		LineOfSight = !Player.GetWorld()->LineTraceTestByChannel(CameraPosition, Position, ECC_Visibility, Params);
	}
	return LineOfSight.GetValue();
}
