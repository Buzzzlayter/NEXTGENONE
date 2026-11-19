#include "Map/UIMarkerSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Map/UIMarkerModel.h"
#include "Map/UIMarkerObserver.h"
#include "Map/UIMarkerReplicator.h"

bool UUIMarkerSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UUIMarkerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ActorPreSpawnHandle = GetWorld()->AddOnActorPreSpawnInitialization(
		FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::BindMarkerReplicator));
}

void UUIMarkerSubsystem::BindMarkerReplicator(AActor* Actor)
{
	auto* Receiver = Cast<AUIMarkerReplicator>(Actor);
	if (!Receiver || GetWorld()->GetNetMode() != NM_Client) return;

	Replicator = Receiver;
	Receiver->OnMarkerReceived.AddUObject(this, &ThisClass::AddMarker);
	Receiver->OnMarkerRemoved.AddUObject(this, &ThisClass::RemoveMarker);
}

void UUIMarkerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const ENetMode NetMode = InWorld.GetNetMode();
	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
		Replicator = InWorld.SpawnActor<AUIMarkerReplicator>();
	if (NetMode == NM_DedicatedServer) return;
	auto* GameInstance = InWorld.GetGameInstance();
	if (!GameInstance) return;
	GameInstance->OnLocalPlayerAddedEvent.AddUObject(this, &ThisClass::AddLocalObserver);
	GameInstance->OnLocalPlayerRemovedEvent.AddUObject(this, &ThisClass::RemoveLocalObserver);
	for (auto* Player : GameInstance->GetLocalPlayers()) AddLocalObserver(Player);
}

void UUIMarkerSubsystem::Deinitialize()
{
	if (auto* World = GetWorld())
	{
		World->RemoveOnActorPreSpawnInitialization(ActorPreSpawnHandle);
		if (auto* GameInstance = World->GetGameInstance())
		{
			GameInstance->OnLocalPlayerAddedEvent.RemoveAll(this);
			GameInstance->OnLocalPlayerRemovedEvent.RemoveAll(this);
		}
	}
	for (const auto& Pair : MarkersBySource)
		if (auto* Source = Pair.Key.Get()) Source->OnEndPlay.RemoveDynamic(this, &ThisClass::OnMarkerSourceEndPlay);
	MarkersBySource.Reset();
	if (auto* MarkerReplicator = Replicator.Get())
	{
		MarkerReplicator->OnMarkerReceived.RemoveAll(this);
		MarkerReplicator->OnMarkerRemoved.RemoveAll(this);
	}
	Replicator.Reset();
	auto Observers = MoveTemp(MapObservers);
	for (const auto& Pair : Observers)
	{
		Pair.Key->OnPlayerControllerChanged().RemoveAll(this);
		Pair.Value->Reset();
	}
	Markers.Reset();
	Super::Deinitialize();
}

UUIMarkerObserver* UUIMarkerSubsystem::GetMarkerObserver(ULocalPlayer* Player) const
{
	return MapObservers.FindRef(Player).Get();
}

void UUIMarkerSubsystem::AddLocalObserver(ULocalPlayer* Player)
{
	if (!Player || MapObservers.Contains(Player)) return;
	auto* Observer = NewObject<UUIMarkerObserver>(this);
	Observer->Initialize(*GetWorld(), *Player);
	MapObservers.Add(Player, Observer);
	for (UUIMarkerModel* Model : Markers) Observer->AddMarker(Model);
	Player->OnPlayerControllerChanged().AddUObject(this, &ThisClass::LocalControllerChanged, Player);
}

void UUIMarkerSubsystem::RemoveLocalObserver(ULocalPlayer* Player)
{
	Player->OnPlayerControllerChanged().RemoveAll(this);
	TObjectPtr<UUIMarkerObserver> Observer;
	if (MapObservers.RemoveAndCopyValue(Player, Observer)) Observer->Reset();
}

void UUIMarkerSubsystem::LocalControllerChanged(APlayerController* Controller, ULocalPlayer* Player)
{
	auto* Observer = GetMarkerObserver(Player);
	if (!Observer || (Controller && Controller->GetWorld() != GetWorld())) return;
	Observer->RefreshVisibility(true);
}

bool UUIMarkerSubsystem::RegisterMarker(UUIMarkerModel* Model)
{
	if (!IsValid(Model) || Model->GetOuter() != this || !Model->GetMarkerId().IsValid()
		|| Model->GetPosition().ContainsNaN() || Markers.Contains(Model))
		return false;

	const ENetMode NetMode = GetWorld()->GetNetMode();
	const bool bReplicateMarker = Model->ShouldReplicateMarker();
	if ((bReplicateMarker && NetMode == NM_Client) || (!bReplicateMarker && NetMode == NM_DedicatedServer))
		return false;

	if (bReplicateMarker && NetMode != NM_Standalone)
	{
		auto* Publisher = Replicator.Get();
		if (!Publisher || !Model->Rename(nullptr, Publisher, REN_DoNotDirty | REN_DontCreateRedirectors)) return false;
		if (!Publisher->PublishMarker(Model)) return false;
	}
	AddMarker(Model);
	return true;
}

bool UUIMarkerSubsystem::PlaceMarkerForSource(AActor* Source, UUIMarkerModel* MarkerTemplate, const FVector& Position, FName RecipientGroup)
{
	if (!IsValid(Source) || Source->IsActorBeingDestroyed() || Source->GetWorld() != GetWorld()
		|| !IsValid(MarkerTemplate) || Position.ContainsNaN()) return false;

	auto* Previous = MarkersBySource.FindRef(Source).Get();
	if (Previous && Markers.Contains(Previous) && Previous->GetClass() == MarkerTemplate->GetClass()
		&& Previous->GetRecipientGroup() == RecipientGroup)
	{
		Previous->SetPosition(Position);
		return true;
	}

	auto* Model = UUIMarkerModel::CreateRuntimeModel(this, MarkerTemplate, Position, RecipientGroup);
	if (!RegisterMarker(Model)) return false;

	MarkersBySource.Add(Source, Model);
	Source->OnEndPlay.AddUniqueDynamic(this, &ThisClass::OnMarkerSourceEndPlay);
	if (Previous) UnregisterMarker(Previous);
	return true;
}

void UUIMarkerSubsystem::OnMarkerSourceEndPlay(AActor* Source, EEndPlayReason::Type EndPlayReason)
{
	Source->OnEndPlay.RemoveDynamic(this, &ThisClass::OnMarkerSourceEndPlay);
	TWeakObjectPtr<UUIMarkerModel> Previous;
	if (MarkersBySource.RemoveAndCopyValue(Source, Previous))
		if (auto* Model = Previous.Get()) UnregisterMarker(Model);
}

void UUIMarkerSubsystem::UnregisterMarker(UUIMarkerModel* Model)
{
	if (!IsValid(Model) || !Markers.Contains(Model)) return;

	if (Model->GetOuter() == this)
	{
		RemoveMarker(Model);
		return;
	}

	auto* Publisher = Replicator.Get();
	if (!Publisher || !Publisher->HasAuthority() || Model->GetOuter() != Publisher) return;
	Publisher->WithdrawMarker(Model);
	RemoveMarker(Model);
}

void UUIMarkerSubsystem::AddMarker(UUIMarkerModel* Model)
{
	if (!IsValid(Model) || Markers.Contains(Model)) return;
	Markers.Add(Model);
	TArray<TObjectPtr<UUIMarkerObserver>, TInlineAllocator<4>> Observers;
	MapObservers.GenerateValueArray(Observers);
	for (UUIMarkerObserver* Observer : Observers) Observer->AddMarker(Model);
}

void UUIMarkerSubsystem::RemoveMarker(UUIMarkerModel* Model)
{
	if (!Markers.Remove(Model)) return;
	TArray<TObjectPtr<UUIMarkerObserver>, TInlineAllocator<4>> Observers;
	MapObservers.GenerateValueArray(Observers);
	for (UUIMarkerObserver* Observer : Observers) Observer->RemoveMarker(Model);
}
