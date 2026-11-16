#include "Gameplay/Interaction/NGItemPickup.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

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
	return bAvailable && ItemClass && StackCount > 0;
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
