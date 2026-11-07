#include "Gameplay/Interaction/NGItemPickup.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Inventory/NGPlayerInventoryManagerComponent.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ANGItemPickup::ANGItemPickup()
{
	bReplicates=true; SetReplicateMovement(true);
	InteractionCollision=CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision")); SetRootComponent(InteractionCollision);
	InteractionCollision->InitSphereRadius(24.f); InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetGenerateOverlapEvents(false);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore); InteractionCollision->SetCollisionResponseToChannel(NGInteractionChannel,ECR_Block);
	Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); Mesh->SetupAttachment(InteractionCollision); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PickupSkeletalMesh")); SkeletalMesh->SetupAttachment(InteractionCollision); SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ANGItemPickup::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); OnRep_Payload(); OnRep_Available(); }
void ANGItemPickup::BeginPlay() { Super::BeginPlay(); OnRep_Payload(); OnRep_Available(); }
void ANGItemPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ANGItemPickup,ItemClass);
	DOREPLIFETIME(ANGItemPickup,StackCount); DOREPLIFETIME(ANGItemPickup,bAvailable);
}
void ANGItemPickup::OnRep_Payload()
{
	const auto* Def=ItemClass ? GetDefault<UNGInventoryItemDefinition>(ItemClass) : nullptr;
	UStaticMesh* Static=Def ? Def->PickupMesh.Get() : nullptr;
	USkeletalMesh* Skel=Def ? Def->PickupSkeletalMesh.Get() : nullptr;
	if (const auto* Weapon=UNGCombatComponent::GetWeaponDefinition(Def); Weapon && !Static && !Skel) { Static=Weapon->Mesh; Skel=Weapon->SkeletalMesh; }
	FTransform Visual=Def ? Def->PickupTransform : FTransform::Identity;
	Mesh->SetStaticMesh(Static); SkeletalMesh->SetSkeletalMeshAsset(Static ? nullptr : Skel);
	Mesh->SetRelativeTransform(Visual); SkeletalMesh->SetRelativeTransform(Visual);
}
void ANGItemPickup::OnRep_Available()
{
	SetActorHiddenInGame(!bAvailable); SetActorEnableCollision(bAvailable);
	if (!bAvailable) { Mesh->SetRenderCustomDepth(false); SkeletalMesh->SetRenderCustomDepth(false); }
}
FText ANGItemPickup::GetInteractionPrompt_Implementation(APawn* Interactor) const
{
	const auto* Def=ItemClass ? GetDefault<UNGInventoryItemDefinition>(ItemClass) : nullptr;
	return Def ? FText::Format(NSLOCTEXT("NEXTGENONE","PickupPrompt","Взять {0} ×{1}"),Def->DisplayName,FText::AsNumber(StackCount)) : FText::GetEmpty();
}
bool ANGItemPickup::CanInteract_Implementation(APawn* Interactor) const
{
	const auto* Interaction = IsValid(Interactor) ? Interactor->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	if (!bAvailable || !ItemClass || StackCount<1 || !Interaction || !Interaction->IsCurrentInteractor(false)) return false;
	return FindDestinationSlot(Interactor) != INDEX_NONE;
}
int32 ANGItemPickup::FindDestinationSlot(const APawn* Interactor) const
{
	if (!Interactor || !ItemClass) return INDEX_NONE;
	const auto* Player = Interactor->GetPlayerState<APlayerState>();
	const auto* Inventory = Player ? Player->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
	if (!Inventory) return INDEX_NONE;
	const auto* Combat = Interactor->FindComponentByClass<UNGCombatComponent>();
	if (bAutoEquipWhenHandsEmpty && Combat && !Combat->GetEquipment().bEquipped
		&& UNGCombatComponent::GetWeaponDefinition(GetDefault<UNGInventoryItemDefinition>(ItemClass)))
	{
		const int32 ActionSlot = Inventory->FindFirstAvailableSlot(ItemClass, StackCount, TAG_NGSlotAction);
		if (ActionSlot != INDEX_NONE) return ActionSlot;
	}
	// Full Action bar: store the pickup without auto-equipping or replacing another weapon.
	return Inventory->FindFirstAvailableSlot(ItemClass, StackCount, TAG_NGSlotStorage);
}
bool ANGItemPickup::Interact_Implementation(APawn* Interactor)
{
	if (!IsValid(this) || !HasAuthority() || !IsExecutingInteraction(Interactor) || !bAvailable || bTransferring) return false;
	TGuardValue<bool> Guard(bTransferring, true);
	auto* Player = Interactor->GetPlayerState<APlayerState>();
	auto* Inventory = Player ? Player->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
	if (!Inventory || !Inventory->InitializeInventory()) return false;
	const int32 Slot = FindDestinationSlot(Interactor);
	if (Slot == INDEX_NONE) return false;
	const FNGItemState State = GetStateForTransfer();
	auto* Item = HasSavedItemState() ? Inventory->AddItemState(State, false, Slot)
		: Inventory->AddItemDefinitionDeferred(State.ItemClass, State.Count, Slot);
	if (!Item) return false;
	ConsumeForTransfer();
	Inventory->NotifyInventoryChanged();
	return true;
}
void ANGItemPickup::SetInteractionFocus_Implementation(APawn* Interactor,bool bFocused)
{
	Mesh->SetRenderCustomDepth(bFocused && bAvailable); SkeletalMesh->SetRenderCustomDepth(bFocused && bAvailable);
}
void ANGItemPickup::InitializeDroppedState(const FNGItemState& State)
{
	if (!HasAuthority()) return;
	SavedState=State; bHasSavedState=true; ItemClass=State.ItemClass; StackCount=State.Count;
	bAvailable=false; OnRep_Payload(); OnRep_Available();
}
void ANGItemPickup::PublishDrop() { if (HasAuthority()) { bAvailable=true; OnRep_Available(); ForceNetUpdate(); } }
FNGItemState ANGItemPickup::GetStateForTransfer() const
{
	if (bHasSavedState) return SavedState;
	FNGItemState State; State.ItemClass=ItemClass; State.Count=StackCount; return State;
}
void ANGItemPickup::ConsumeForTransfer()
{
	if (!HasAuthority()) return;
	bAvailable=false; StackCount=0; OnRep_Available(); ForceNetUpdate(); SetLifeSpan(.3f);
}
