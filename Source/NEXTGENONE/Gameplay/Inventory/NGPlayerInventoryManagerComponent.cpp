#include "NGPlayerInventoryManagerComponent.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"


UNGPlayerInventoryManagerComponent::UNGPlayerInventoryManagerComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UNGPlayerInventoryManagerComponent::TryBeginStarterGrant()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || bStarterGrantStarted) return false;
    bStarterGrantStarted = true;
    return true;
}

void UNGPlayerInventoryManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Player = Cast<APlayerState>(GetOwner()))
        Player->OnPawnSet.AddUniqueDynamic(this, &ThisClass::PawnChanged);
    else UE_LOG(LogTemp, Error, TEXT("PlayerInventory requires a PlayerState owner: %s"), *GetNameSafe(GetOwner()));
}

void UNGPlayerInventoryManagerComponent::EndPlay(EEndPlayReason::Type Reason)
{
    if (auto* Player = Cast<APlayerState>(GetOwner()))
        Player->OnPawnSet.RemoveDynamic(this, &ThisClass::PawnChanged);
    ClearViewedContext();
    ViewedInventory.Reset();
    ViewedPawn.Reset();
    Super::EndPlay(Reason);
}

void UNGPlayerInventoryManagerComponent::PawnChanged(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
    ClearViewedContext();
    ViewedInventory.Reset();
    ViewedPawn.Reset();
}

APlayerController* UNGPlayerInventoryManagerComponent::GetPlayerController() const
{
    const auto* Player = Cast<APlayerState>(GetOwner());
    return Player ? Player->GetPlayerController() : nullptr;
}
APawn* UNGPlayerInventoryManagerComponent::GetInventoryPawn() const
{
    const auto* Player = Cast<APlayerState>(GetOwner());
    const auto* PC = GetPlayerController();
    auto* CurrentPawn = Player ? Player->GetPawn() : nullptr;
    return PC && IsValid(CurrentPawn) && PC->GetPawn() == CurrentPawn
        && CurrentPawn->GetPlayerState() == Player && PC->PlayerState == Player ? CurrentPawn : nullptr;
}
bool UNGPlayerInventoryManagerComponent::IsLocalController() const { const auto* PC = GetPlayerController(); return PC && PC->IsLocalController(); }

void UNGPlayerInventoryManagerComponent::BindInteractionSession(APawn* Pawn)
{
    if (SessionInteraction.IsValid()) SessionInteraction->OnInteractionEnded.RemoveAll(this);
    SessionInteraction = Pawn ? Pawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
    if (SessionInteraction.IsValid()) SessionInteraction->OnInteractionEnded.AddUObject(this, &ThisClass::InteractionEnded);
}

void UNGPlayerInventoryManagerComponent::InteractionEnded(UObject* Source)
{
    if (Source == ViewedInventory.Get()) CloseInventoryView();
}

void UNGPlayerInventoryManagerComponent::ClearViewedContext()
{
    auto* Interaction = SessionInteraction.Get();
    if (Interaction) Interaction->OnInteractionEnded.RemoveAll(this);
    SessionInteraction.Reset();
    if (Interaction) Interaction->ClearContext(ViewedInventory.Get());
}
void UNGPlayerInventoryManagerComponent::RequestDropItem(UNGInventoryItemInstance* Item,int32 Count)
{
	if (!IsLocalController() && !GetOwner()->HasAuthority()) return;
	auto* Expected=Cast<ANextgenoneCharacter>(GetInventoryPawn());
	if (GetOwner()->HasAuthority()) ServerDropItem_Implementation(Expected,Item,Count); else ServerDropItem(Expected,Item,Count);
}
void UNGPlayerInventoryManagerComponent::ServerDropItem_Implementation(ANextgenoneCharacter* ExpectedPawn,UNGInventoryItemInstance* Item,int32 Count)
{
	if (!ExpectedPawn || GetInventoryPawn()!=ExpectedPawn) return;
	auto* System=GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	if (!System || !System->IsCurrentInteractor(ExpectedPawn)) return;
	const double Now=GetWorld()->GetTimeSeconds(); if (Now<NextDropTime) return;
	NextDropTime=Now+.15; System->DropInventoryItem(ExpectedPawn,Item,Count);
}
void UNGPlayerInventoryManagerComponent::OpenInventorySource(UNGInventoryManagerComponent* Source, FGameplayTag InteractionType)
{
	auto* CharacterPawn = Cast<ANextgenoneCharacter>(GetInventoryPawn());
	auto* System = GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	if (!GetOwner()->HasAuthority() || !InteractionType.IsValid() || !IsValid(Source) || !CharacterPawn || !System || !System->IsExecutingInteraction(Source->GetOwner(), CharacterPawn)) return;
	ViewedInventory = Source;
	ViewedPawn = CharacterPawn;
	BindInteractionSession(CharacterPawn);
	if (SessionInteraction.IsValid()) SessionInteraction->SetConfirmedContext(InteractionType, Source->GetOwner(), Source);
	if (GetViewedInventory() == Source) ClientShowInventory(CharacterPawn, Source, InteractionType);
}
UNGInventoryManagerComponent* UNGPlayerInventoryManagerComponent::GetViewedInventory() const
{
	const auto* CharacterPawn = Cast<ANextgenoneCharacter>(GetInventoryPawn());
	return CharacterPawn && !CharacterPawn->IsDead() && ViewedPawn.Get() == CharacterPawn ? ViewedInventory.Get() : nullptr;
}
void UNGPlayerInventoryManagerComponent::ClientShowInventory_Implementation(ANextgenoneCharacter* ExpectedPawn, UNGInventoryManagerComponent* Source, FGameplayTag InteractionType)
{
	if (!IsValid(ExpectedPawn) || GetInventoryPawn() != ExpectedPawn || !IsValid(Source)) return;
	ViewedInventory = Source;
	ViewedPawn = ExpectedPawn;
	BindInteractionSession(ExpectedPawn);
	if (auto* Interaction = ExpectedPawn->FindComponentByClass<UNGInteractionComponent>())
		Interaction->SetConfirmedContext(InteractionType, Source->GetOwner(), Source);
}
void UNGPlayerInventoryManagerComponent::CloseInventoryView()
{
    APawn* SessionPawn = ViewedPawn.Get();
    auto* SessionSource = ViewedInventory.Get();
    ClearViewedContext();
    ViewedInventory.Reset(); ViewedPawn.Reset();
    if (GetOwner()->HasAuthority()) ClientCloseInventoryView(SessionPawn, SessionSource);
    else ServerCloseInventoryView(SessionPawn, SessionSource);
}

void UNGPlayerInventoryManagerComponent::ServerCloseInventoryView_Implementation(APawn* ExpectedPawn, UNGInventoryManagerComponent* ExpectedSource)
{
    if (ViewedPawn.Get() != ExpectedPawn || ViewedInventory.Get() != ExpectedSource) return;
    if (auto* Interaction = SessionInteraction.Get()) Interaction->EndInteraction();
    // EndInteraction normally closes through the callback; tolerate a context already cleared.
    if (ViewedPawn.Get() == ExpectedPawn && ViewedInventory.Get() == ExpectedSource) CloseInventoryView();
}

void UNGPlayerInventoryManagerComponent::ClientCloseInventoryView_Implementation(APawn* ExpectedPawn, UNGInventoryManagerComponent* ExpectedSource)
{
    if (ViewedPawn.Get() != ExpectedPawn || ViewedInventory.Get() != ExpectedSource) return;
    ClearViewedContext();
    ViewedInventory.Reset(); ViewedPawn.Reset();
}

bool UNGPlayerInventoryManagerComponent::CanAccessInventory(UNGInventoryManagerComponent* Inventory) const
{
	if (!IsValid(Inventory) || !IsValid(Inventory->GetOwner())) return false;
	if (Inventory->GetOwner() == (GetPlayerController() ? GetPlayerController()->GetPlayerState<APlayerState>() : nullptr) || Inventory->GetOwner() == GetInventoryPawn()) return true;
	if (GetViewedInventory() != Inventory) return false;
	auto* CharacterPawn = Cast<ANextgenoneCharacter>(GetInventoryPawn());
	auto* System = GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	return CharacterPawn && CharacterPawn->Interaction && System && System->IsCurrentInteractor(CharacterPawn) && System->ValidateTarget(CharacterPawn, Inventory->GetOwner(), CharacterPawn->Interaction->MaxDistance)
		&& Inventory->GetOwner()->GetClass()->ImplementsInterface(UNGInteractable::StaticClass())
		&& INGInteractable::Execute_CanInteract(Inventory->GetOwner(), CharacterPawn);
}
void UNGPlayerInventoryManagerComponent::RequestInventoryTransfer(UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount)
{
	if (!IsLocalController() && !GetOwner()->HasAuthority()) return;
	auto* CharacterPawn = Cast<ANextgenoneCharacter>(GetInventoryPawn());
	if (GetOwner()->HasAuthority()) ServerInventoryTransfer_Implementation(CharacterPawn, Source, ExpectedItem, SourceSlot, Target, TargetSlot, ExpectedTarget, Amount);
	else ServerInventoryTransfer(CharacterPawn, Source, ExpectedItem, SourceSlot, Target, TargetSlot, ExpectedTarget, Amount);
}
void UNGPlayerInventoryManagerComponent::ServerInventoryTransfer_Implementation(ANextgenoneCharacter* ExpectedPawn, UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount)
{
	if (!ExpectedPawn || GetInventoryPawn() != ExpectedPawn || ExpectedPawn->IsDead() || !FMath::IsFinite(InventoryOperationInterval)
		|| !CanAccessInventory(Source) || !CanAccessInventory(Target) || !IsValid(ExpectedItem)
		|| Source->GetItemAtSlot(SourceSlot) != ExpectedItem || Target->GetItemAtSlot(TargetSlot) != ExpectedTarget) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextOperationTime) return;
	NextOperationTime = Now + FMath::Max(.01f, InventoryOperationInterval);
	Source->TransferTo(Target, SourceSlot, TargetSlot, Amount);
}
