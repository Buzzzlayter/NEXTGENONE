#include "NGInventoryBox.h"
#include "NativeGameplayTags.h"

#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "NGPlayerInventoryManagerComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGInteractionStorage, "Interaction.Storage");

ANGInventoryBox::ANGInventoryBox()
{
	bReplicates = true;
	InteractionType = TAG_NGInteractionStorage;
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetBoxExtent(FVector(40.f));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(ECC_WorldStatic);
	Collision->SetCollisionResponseToChannel(NGInteractionChannel, ECR_Block);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Inventory = CreateDefaultSubobject<UNGInventoryManagerComponent>(TEXT("Inventory"));
	Inventory->bOwnerOnly = false;
}
void ANGInventoryBox::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) for (const auto& Seed : InitialContents)
		if (!Inventory->ImportItemState(Seed.Item, Seed.SlotIndex))
			UE_LOG(LogTemp, Warning, TEXT("Inventory box %s: rejected seed for slot %d"), *GetName(), Seed.SlotIndex);
}
FText ANGInventoryBox::GetInteractionPrompt_Implementation(ANextgenoneCharacter* Interactor) const { return Title; }
bool ANGInventoryBox::CanInteract_Implementation(ANextgenoneCharacter* Interactor) const
{
	return IsValid(Interactor) && !Interactor->IsDead() && Interactor->GetController() && IsValid(Inventory) && InteractionType.IsValid();
}
bool ANGInventoryBox::Interact_Implementation(ANextgenoneCharacter* Interactor)
{
	auto* System = GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	auto* Player = Interactor ? Interactor->GetPlayerState<APlayerState>() : nullptr;
	auto* Transport = Player ? Player->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
	if (!HasAuthority() || !Transport || !System || !System->IsExecutingInteraction(this, Interactor) || !CanInteract_Implementation(Interactor)) return false;
	Transport->OpenInventorySource(Inventory, InteractionType);
	return true;
}
