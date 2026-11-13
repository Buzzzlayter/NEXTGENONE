#include "NGPlayerInventoryManagerComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "Gameplay/Abilities/NGItemActivateAbility.h"
#include "Gameplay/Interaction/NGItemPickup.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/NGInventoryFragment_ItemActions.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Equipment/NGEquipmentDefinition.h"

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
	const auto* Controller = Player ? Cast<AController>(Player->GetOwner()) : nullptr;
	auto* CurrentPawn = Player ? Player->GetPawn() : nullptr;
	return Controller && IsValid(CurrentPawn) && Controller->GetPawn() == CurrentPawn
		&& CurrentPawn->GetController() == Controller && CurrentPawn->GetPlayerState() == Player
		&& Controller->GetPlayerState<APlayerState>() == Player ? CurrentPawn : nullptr;
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
	auto* Expected = GetInventoryPawn();
	if (GetOwner()->HasAuthority()) ServerDropItem_Implementation(Expected, Item, Count);
	else ServerDropItem(Expected, Item, Count);
	return true;
}

void UNGPlayerInventoryManagerComponent::ServerDropItem_Implementation(APawn* ExpectedPawn, UNGInventoryItemInstance* Item, int32 Count)
{
	if (Count < 0) return;
	if (!ExpectedPawn || GetInventoryPawn() != ExpectedPawn) return;
	auto* Interaction = ExpectedPawn->FindComponentByClass<UNGInteractionComponent>();
	if (!Interaction || !Interaction->IsCurrentInteractor()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextDropTime) return;
	NextDropTime = Now + .15;
	if (!IsValid(Item) || !Contains(Item)) return;
	DropItemOnServer(ExpectedPawn, Item, Count);
}

void UNGPlayerInventoryManagerComponent::OpenInventorySource(UNGInventoryManagerComponent* Source, FGameplayTag InteractionType)
{
	auto* PlayerPawn = GetInventoryPawn();
	const auto* Target = IsValid(Source) ? Cast<ANGInteractableActor>(Source->GetOwner()) : nullptr;
	if (!GetOwner()->HasAuthority() || !InteractionType.IsValid() || !PlayerPawn || !Target || !Target->IsExecutingInteraction(PlayerPawn)) return;
	ViewedInventory = Source;
	ViewedPawn = PlayerPawn;
	BindInteractionSession(PlayerPawn);
	if (SessionInteraction.IsValid()) SessionInteraction->SetConfirmedContext(InteractionType, Source->GetOwner(), Source);
	if (GetViewedInventory() == Source) ClientShowInventory(PlayerPawn, Source, InteractionType);
}
UNGInventoryManagerComponent* UNGPlayerInventoryManagerComponent::GetViewedInventory() const
{
	const auto* PlayerPawn = GetInventoryPawn();
	const auto* Interaction = PlayerPawn ? PlayerPawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	return Interaction && Interaction->IsCurrentInteractor(false) && ViewedPawn.Get() == PlayerPawn ? ViewedInventory.Get() : nullptr;
}
void UNGPlayerInventoryManagerComponent::ClientShowInventory_Implementation(APawn* ExpectedPawn, UNGInventoryManagerComponent* Source, FGameplayTag InteractionType)
{
	if (!IsValid(ExpectedPawn) || GetInventoryPawn() != ExpectedPawn || !IsValid(Source)) return;
	auto* Interaction = ExpectedPawn->FindComponentByClass<UNGInteractionComponent>();
	if (!Interaction || !Interaction->IsCurrentInteractor(false)) return;
	ViewedInventory = Source;
	ViewedPawn = ExpectedPawn;
	BindInteractionSession(ExpectedPawn);
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
	if (Inventory->GetOwner() == GetOwner() || Inventory->GetOwner() == GetInventoryPawn()) return true;
	if (GetViewedInventory() != Inventory) return false;
	auto* PlayerPawn = GetInventoryPawn();
	const auto* Interaction = PlayerPawn ? PlayerPawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	return Interaction && Interaction->IsCurrentInteractor() && Interaction->ValidateTarget(Inventory->GetOwner())
		&& Inventory->GetOwner()->GetClass()->ImplementsInterface(UNGInteractable::StaticClass())
		&& INGInteractable::Execute_CanInteract(Inventory->GetOwner(), PlayerPawn);
}
bool UNGPlayerInventoryManagerComponent::RequestInventoryTransfer(UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount)
{
	if (!IsLocalController() && !GetOwner()->HasAuthority()) return false;
	auto* PlayerPawn = GetInventoryPawn();
	if (GetOwner()->HasAuthority()) ServerInventoryTransfer_Implementation(PlayerPawn, Source, ExpectedItem, SourceSlot, Target, TargetSlot, ExpectedTarget, Amount);
	else ServerInventoryTransfer(PlayerPawn, Source, ExpectedItem, SourceSlot, Target, TargetSlot, ExpectedTarget, Amount);
	return true;
}

void UNGPlayerInventoryManagerComponent::ServerInventoryTransfer_Implementation(APawn* ExpectedPawn, UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount)
{
	if (Amount < 0 || SourceSlot < 0 || TargetSlot < 0 || !FMath::IsFinite(InventoryOperationInterval))
	return;
	if (!ExpectedPawn || GetInventoryPawn() != ExpectedPawn)
	return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextOperationTime) return;
	NextOperationTime = Now + FMath::Max(.01f, InventoryOperationInterval);
	const auto* Interaction = ExpectedPawn->FindComponentByClass<UNGInteractionComponent>();
	if (!Interaction || !Interaction->IsCurrentInteractor()) return;
	if (!CanAccessInventory(Source)) return;
	if (!CanAccessInventory(Target)) return;
	if (!IsValid(ExpectedItem) || Source->GetItemAtSlot(SourceSlot) != ExpectedItem || Target->GetItemAtSlot(TargetSlot) != ExpectedTarget)
	return;
	ExecuteTransferOnServer(Source, SourceSlot, Target, TargetSlot, Amount);
}

void UNGPlayerInventoryManagerComponent::GrantStarterInventory()
{
	if (!TryBeginStarterGrant()) return;
	int32 Failures = 0;
	for (const auto& Entry : InitialContents)
	{
		const int32 Destination = FindFirstAvailableSlot(Entry.ItemClass, Entry.Count, Entry.SlotTag);
		if (Destination == INDEX_NONE || !AddItemDefinitionToSlot(Entry.ItemClass, Destination, Entry.Count))
		{
			++Failures;
			UE_LOG(LogTemp, Error, TEXT("%s: rejected starter item %s count=%d group=%s"),
				*GetPathName(), *GetNameSafe(Entry.ItemClass.Get()), Entry.Count, *Entry.SlotTag.ToString());
		}
	}
	// Preserve successful entries, report partial failure, never retry implicitly on respawn.
	if (Failures) UE_LOG(LogTemp, Error, TEXT("%s: starter inventory partially granted (%d failures); no automatic retry."), *GetPathName(), Failures);
}

bool UNGPlayerInventoryManagerComponent::RequestActivationItem(UNGInventoryItemInstance* Item, UNGInventoryManagerComponent* Source)
{
	if (!IsLocalController() && !GetOwner()->HasAuthority()) return false;
	if (!Source) Source = this;
	if (!IsValid(Source)) return false;
	if (GetOwner()->HasAuthority()) ServerActivationItem_Implementation(GetInventoryPawn(), Source, Item, Source->GetSlotIndex(Item));
	else ServerActivationItem(GetInventoryPawn(), Source, Item, Source->GetSlotIndex(Item));
	return true;
}

bool UNGPlayerInventoryManagerComponent::RequestActivationSlot(int32 ActionSlot)
{
	const int32 Slot = FindSlotByTag(TAG_NGSlotAction, ActionSlot);
	auto* Item = GetItemAtSlot(Slot);
	return Item && RequestActivationItem(Item);
}

void UNGPlayerInventoryManagerComponent::ServerActivationItem_Implementation(APawn* ExpectedPawn, UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* Item, int32 ExpectedSlot)
{
	if (!ExpectedPawn || ExpectedPawn != GetInventoryPawn()) return;
	if (!CanAccessInventory(Source) || !IsValid(Item) || ExpectedSlot < 0 || Source->GetItemAtSlot(ExpectedSlot) != Item) return;
	if (Source != this)
	{
		// External secondary action is a transfer into the item's typed equipment slot.
		const auto* Definition = UNGEquipmentComponent::GetDefinition(Item);
		if (!Definition || !Definition->SlotTag.MatchesTag(TAG_NGSlotEquipment)) return;
		const int32 Destination = FindSlotByTag(Definition->SlotTag, 0);
		if (Destination == INDEX_NONE || !FMath::IsFinite(InventoryOperationInterval)) return;
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now < NextOperationTime) return;
		NextOperationTime = Now + FMath::Max(.01f, InventoryOperationInterval);
		ExecuteTransferOnServer(Source, ExpectedSlot, this, Destination, 0);
		return;
	}
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

ANGItemPickup* UNGPlayerInventoryManagerComponent::DropItemOnServer(APawn* Pawn,UNGInventoryItemInstance* Item,int32 Count)
{
	auto* Interaction = IsValid(Pawn) ? Pawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	if (GetInventoryPawn() != Pawn || !Interaction || !Interaction->IsCurrentInteractor() || !IsValid(Item) || ActiveDrops.Contains(Item)) return nullptr;
	const int32 OldCount=GetCount(Item); if (Count==0) Count=OldCount;
	if (Count<1 || Count>OldCount) return nullptr;
	const auto State=Item->CaptureState(Count);
	if (!State.ItemClass) return nullptr;
	// Stateful stacks need an explicit split policy; do not duplicate ammo/charges into two instances.
	if (Count<OldCount && !State.CanSplitStack()) return nullptr;
	ActiveDrops.Add(Item); ON_SCOPE_EXIT { ActiveDrops.Remove(Item); };
	const auto* Movement = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());
	const FVector Up = Movement ? -Movement->GetGravityDirection() : Pawn->GetActorUpVector();
	const FVector Forward=FVector::VectorPlaneProject(Pawn->GetActorForwardVector(),Up).GetSafeNormal();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGDrop),false,Pawn);
	FVector Position=Pawn->GetActorLocation()+Forward*140.f;
	FHitResult Wall; if (GetWorld()->SweepSingleByChannel(Wall,Pawn->GetActorLocation(),Position,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(26.f),Params)) Position=Wall.Location-Forward*28.f;
	FHitResult Ground; if (!GetWorld()->LineTraceSingleByChannel(Ground,Position,Position-Up*600.f,ECC_Visibility,Params)) return nullptr;
	Position=Ground.ImpactPoint+Up*28.f;
	if (GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(24.f),Params)) return nullptr;
	if (FVector::DistSquared(Position,Pawn->GetActorLocation())>FMath::Square(650.f)) return nullptr;
	UClass* Class = Interaction->DroppedPickupClass.Get();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
	const FTransform Transform(FRotationMatrix::MakeFromXZ(Forward,Up).Rotator(),Position);
	auto* Pickup=GetWorld()->SpawnActorDeferred<ANGItemPickup>(Class,Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup) return nullptr;
	Pickup->InitializeDroppedState(State); Pickup->FinishSpawning(Transform);
	// Construction scripts cannot debit the source or modify it behind the saved snapshot.
	Interaction = IsValid(Pawn) ? Pawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	if (!IsValid(Pickup) || GetInventoryPawn() != Pawn || !Interaction || !Interaction->IsCurrentInteractor() || GetCount(Item)!=OldCount ||
		!Item->CaptureState(Count).StatStacks.OrderIndependentCompareEqual(State.StatStacks) || !RemoveQuantity(Item,Count,false))
	{ if (IsValid(Pickup)) Pickup->Destroy(); return nullptr; }
	Pickup->PublishDrop();
	NotifyInventoryChanged();
	return Pickup;
}

bool UNGPlayerInventoryManagerComponent::MoveEquipmentItemOnServer(UNGInventoryItemInstance* Item, int32 TargetSlot)
{
    return ExecuteTransferOnServer(this, GetSlotIndex(Item), this, TargetSlot, 0);
}
bool UNGPlayerInventoryManagerComponent::ExecuteTransferOnServer(UNGInventoryManagerComponent* Source, int32 SourceSlot,
    UNGInventoryManagerComponent* Target, int32 TargetSlot, int32 Amount)
{
    auto* Pawn = GetInventoryPawn();
    const auto* Interaction = Pawn ? Pawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
    auto* Equipment = Pawn ? Pawn->FindComponentByClass<UNGEquipmentComponent>() : nullptr;
    if (!GetOwner()->HasAuthority() || !Interaction || !Interaction->IsCurrentInteractor() || !Equipment
        || !CanAccessInventory(Source) || !CanAccessInventory(Target)) return false;
    auto* Item = Source->GetItemAtSlot(SourceSlot); auto* Other = Target->GetItemAtSlot(TargetSlot);
    if (!Item) return false;
    FString Error;
    if (Target == this && (!Equipment->PreflightPlacement(Item, TargetSlot, Error)
        || (GetSlotTag(TargetSlot).MatchesTag(TAG_NGSlotEquipment) && (Source->GetCount(Item) != 1 || Amount > 1)))) return false;
    if (Source == this && Other && (!Equipment->PreflightPlacement(Other, SourceSlot, Error)
        || (GetSlotTag(SourceSlot).MatchesTag(TAG_NGSlotEquipment) && Target->GetCount(Other) != 1))) return false;
    return Source->TransferTo(Target, SourceSlot, TargetSlot, Amount);
}
