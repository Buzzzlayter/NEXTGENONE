#include "Gameplay/Interaction/NGItemPickup.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); Mesh->SetStaticMesh(Cube.Object);
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
	if (!Static && !Skel) { Static=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")); Visual.SetScale3D(Visual.GetScale3D()*.3f); }
	Mesh->SetStaticMesh(Static); SkeletalMesh->SetSkeletalMeshAsset(Static ? nullptr : Skel);
	Mesh->SetRelativeTransform(Visual); SkeletalMesh->SetRelativeTransform(Visual);
}
void ANGItemPickup::OnRep_Available()
{
	SetActorHiddenInGame(!bAvailable); SetActorEnableCollision(bAvailable);
	if (!bAvailable) { Mesh->SetRenderCustomDepth(false); SkeletalMesh->SetRenderCustomDepth(false); }
}
FText ANGItemPickup::GetInteractionPrompt_Implementation(ANextgenoneCharacter* Interactor) const
{
	const auto* Def=ItemClass ? GetDefault<UNGInventoryItemDefinition>(ItemClass) : nullptr;
	return Def ? FText::Format(NSLOCTEXT("NEXTGENONE","PickupPrompt","Взять {0} ×{1}"),Def->DisplayName,FText::AsNumber(StackCount)) : FText::GetEmpty();
}
bool ANGItemPickup::CanInteract_Implementation(ANextgenoneCharacter* Interactor) const
{
	if (!bAvailable || !ItemClass || StackCount<1 || !Interactor || Interactor->IsDead()) return false;
	return FindDestinationSlot(Interactor) != INDEX_NONE;
}
int32 ANGItemPickup::FindDestinationSlot(const ANextgenoneCharacter* Interactor) const
{
	if (!Interactor || !ItemClass) return INDEX_NONE;
	const auto* PS=Interactor->GetPlayerState<ANextgenonePlayerState>();
	if (!PS) return INDEX_NONE;
	if (bAutoEquipWhenHandsEmpty && Interactor->Combat && !Interactor->Combat->GetEquipment().bEquipped
		&& UNGCombatComponent::GetWeaponDefinition(GetDefault<UNGInventoryItemDefinition>(ItemClass)))
	{
		const int32 ActionSlot = PS->Inventory->FindFirstAvailableSlot(ItemClass, StackCount, TAG_NGSlotAction);
		if (ActionSlot != INDEX_NONE) return ActionSlot;
	}
	// Full Action bar: store the pickup without auto-equipping or replacing another weapon.
	return PS->Inventory->FindFirstAvailableSlot(ItemClass, StackCount, TAG_NGSlotStorage);
}
bool ANGItemPickup::Interact_Implementation(ANextgenoneCharacter* Interactor)
{
	if (auto* System=GetWorld()->GetSubsystem<UNGInteractionSubsystem>()) return System->TakePickup(this,Interactor);
	return false;
}
void ANGItemPickup::SetInteractionFocus_Implementation(ANextgenoneCharacter* Interactor,bool bFocused)
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
