#include "NGPlayerInventoryManagerComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "Gameplay/Abilities/NGItemActivateAbility.h"
#include "Inventory/NGInventoryFragment_ItemActions.h"

UNGPlayerInventoryManagerComponent::UNGPlayerInventoryManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNGPlayerInventoryManagerComponent::TryBeginStarterGrant()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bStarterGrantStarted || !InitializeInventory()) return false;
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
bool UNGPlayerInventoryManagerComponent::RequestDropItem(UNGInventoryItemInstance* Item, int32 Count)
{
	if (!IsLocalController() && !GetOwner()->HasAuthority()) return false;
	auto* Expected = Cast<ANextgenoneCharacter>(GetInventoryPawn());
	if (GetOwner()->HasAuthority()) ServerDropItem_Implementation(Expected, Item, Count);
	else ServerDropItem(Expected, Item, Count);
	return true;
}

void UNGPlayerInventoryManagerComponent::ServerDropItem_Implementation(ANextgenoneCharacter* ExpectedPawn, UNGInventoryItemInstance* Item, int32 Count)
{
	if (Count < 0) return;
	if (!ExpectedPawn || GetInventoryPawn() != ExpectedPawn) return;
	auto* System = GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	if (!System || !System->IsCurrentInteractor(ExpectedPawn)) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextDropTime) return;
	NextDropTime = Now + .15;
	if (!IsValid(Item) || !Contains(Item)) return;
	System->DropInventoryItem(ExpectedPawn, Item, Count);
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
	return CharacterPawn && NGPlayerContext::GetCurrentASC(CharacterPawn, false) && !CharacterPawn->IsDead()
		&& ViewedPawn.Get() == CharacterPawn ? ViewedInventory.Get() : nullptr;
}
void UNGPlayerInventoryManagerComponent::ClientShowInventory_Implementation(ANextgenoneCharacter* ExpectedPawn, UNGInventoryManagerComponent* Source, FGameplayTag InteractionType)
{
	if (!IsValid(ExpectedPawn) || GetInventoryPawn() != ExpectedPawn || !IsValid(Source)) return;
	const auto* System = GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	if (!System || !System->IsCurrentInteractor(ExpectedPawn, false)) return;
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
bool UNGPlayerInventoryManagerComponent::RequestInventoryTransfer(UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount)
{
	if (!IsLocalController() && !GetOwner()->HasAuthority()) return false;
	auto* CharacterPawn = Cast<ANextgenoneCharacter>(GetInventoryPawn());
	if (GetOwner()->HasAuthority()) ServerInventoryTransfer_Implementation(CharacterPawn, Source, ExpectedItem, SourceSlot, Target, TargetSlot, ExpectedTarget, Amount);
	else ServerInventoryTransfer(CharacterPawn, Source, ExpectedItem, SourceSlot, Target, TargetSlot, ExpectedTarget, Amount);
	return true;
}

void UNGPlayerInventoryManagerComponent::ServerInventoryTransfer_Implementation(ANextgenoneCharacter* ExpectedPawn, UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount)
{
	if (Amount < 0 || SourceSlot < 0 || TargetSlot < 0 || !FMath::IsFinite(InventoryOperationInterval)) return;
	if (!ExpectedPawn || GetInventoryPawn() != ExpectedPawn || ExpectedPawn->IsDead()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextOperationTime) return;
	NextOperationTime = Now + FMath::Max(.01f, InventoryOperationInterval);
	auto* System = GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	if (!System || !System->IsCurrentInteractor(ExpectedPawn) || !CanAccessInventory(Source) || !CanAccessInventory(Target))
	return;
	if (!IsValid(ExpectedItem) || Source->GetItemAtSlot(SourceSlot) != ExpectedItem || Target->GetItemAtSlot(TargetSlot) != ExpectedTarget)
	return;
	Source->TransferTo(Target, SourceSlot, TargetSlot, Amount);
}

void UNGPlayerInventoryManagerComponent::GrantStarterInventory()
{
	if (!TryBeginStarterGrant()) return;
	if (InitialContents.IsEmpty() && !StarterItems.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: migrate legacy StarterItems to explicit InitialContents; legacy items are no longer granted."), *GetPathName());
	}
	int32 Failures = 0;
	for (const auto& Entry : InitialContents)
	{
		const int32 Destination = Entry.SlotIndex == INDEX_NONE
			? FindFirstAvailableSlot(Entry.ItemClass, Entry.Count, Entry.SlotTag) : Entry.SlotIndex;
		const bool bMatchesGroup = !Entry.SlotTag.IsValid() || GetSlotTag(Destination).MatchesTag(Entry.SlotTag);
		if (Destination == INDEX_NONE || !bMatchesGroup || !AddItemDefinitionToSlot(Entry.ItemClass, Destination, Entry.Count))
		{
			++Failures;
			UE_LOG(LogTemp, Error, TEXT("%s: rejected starter item %s count=%d slot=%d group=%s"),
				*GetPathName(), *GetNameSafe(Entry.ItemClass.Get()), Entry.Count, Entry.SlotIndex, *Entry.SlotTag.ToString());
		}
	}
	// Preserve successful entries, report partial failure, never retry implicitly on respawn.
	if (Failures) UE_LOG(LogTemp, Error, TEXT("%s: starter inventory partially granted (%d failures); no automatic retry."), *GetPathName(), Failures);
}

bool UNGPlayerInventoryManagerComponent::RequestActivationItem(UNGInventoryItemInstance* Item)
{
	if (!IsLocalController() && !GetOwner()->HasAuthority()) return false;
	if (GetOwner()->HasAuthority()) ServerActivationItem_Implementation(GetInventoryPawn(), Item, GetSlotIndex(Item));
	else ServerActivationItem(GetInventoryPawn(), Item, GetSlotIndex(Item));
	return true;
}

bool UNGPlayerInventoryManagerComponent::RequestActivationSlot(int32 ActionSlot)
{
	const int32 Slot = FindSlotByTag(TAG_NGSlotAction, ActionSlot);
	auto* Item = GetItemAtSlot(Slot);
	return Item && RequestActivationItem(Item);
}

void UNGPlayerInventoryManagerComponent::ServerActivationItem_Implementation(APawn* ExpectedPawn, UNGInventoryItemInstance* Item, int32 ExpectedSlot)
{
	if (!ExpectedPawn || ExpectedPawn != GetInventoryPawn()) return;
	if (!IsValid(Item) || ExpectedSlot < 0 || GetItemAtSlot(ExpectedSlot) != Item) return;
	ActivateItemOnServer(Item);
}

bool UNGPlayerInventoryManagerComponent::ActivateItemOnServer(UNGInventoryItemInstance* Item)
{
	auto* CurrentPawn = GetInventoryPawn();
	auto* ASC = NGPlayerContext::GetCurrentASC(CurrentPawn, true);
	if (!ASC || !IsValid(Item) || !Contains(Item) || GetCount(Item) < 1 || !Item->GetItemDef()
		|| ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) <= 0.f
		|| ASC->HasMatchingGameplayTag(TAG_NGDead) || ASC->HasMatchingGameplayTag(TAG_NGActivatingItem)) return false;
	const auto* Actions = Item->FindFragmentByClass<UNGInventoryFragment_ItemActions>();
	TSubclassOf<UGameplayAbility> AbilityClass;
	if (Actions) AbilityClass = Actions->ActivationAbility;
	if (!AbilityClass || !AbilityClass->IsChildOf(UNGItemActivateAbility::StaticClass()) || AbilityClass->HasAnyClassFlags(CLASS_Abstract)
		|| AbilityClass.GetDefaultObject()->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::ServerOnly
		|| AbilityClass.GetDefaultObject()->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerActor) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextActivationTime) return false;
	NextActivationTime = Now + .15;
	// Repeated requests cannot grant parallel copies of an already active action.
	for (const auto& Spec : ASC->GetActivatableAbilities())
		if (Spec.SourceObject.Get() == Item && Spec.IsActive()) return false;
	FGameplayAbilitySpec Spec(AbilityClass, 1); Spec.SourceObject = Item;
	return ASC->GiveAbilityAndActivateOnce(Spec).IsValid();
}
