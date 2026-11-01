#include "NGItemPickup.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "NextgenoneCharacter.h"
#include "NextgenonePlayerState.h"
#include "NGCombat.h"
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
	if (Def && Def->Weapon && !Static && !Skel) { Static=Def->Weapon->Mesh; Skel=Def->Weapon->SkeletalMesh; }
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
	const auto* PS=Interactor->GetPlayerState<ANextgenonePlayerState>();
	if (!PS) return false;
	if (HasAuthority()) return PS->Inventory->CanAddItemDefinition(ItemClass,StackCount);
	return PS->Inventory->GetAllItems().Num()<PS->Inventory->MaxEntries && StackCount<=GetDefault<UNGInventoryItemDefinition>(ItemClass)->MaxStackSize;
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
