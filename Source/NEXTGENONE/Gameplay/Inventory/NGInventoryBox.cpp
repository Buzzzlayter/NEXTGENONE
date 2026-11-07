#include "NGInventoryBox.h"
#include "NativeGameplayTags.h"

#include "GameFramework/Pawn.h"
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
		if (!Inventory->ImportItemState(Seed.Item))
			UE_LOG(LogTemp, Warning, TEXT("Inventory box %s: no compatible free slot or invalid initial item"), *GetName());
}
FText ANGInventoryBox::GetInteractionPrompt_Implementation(APawn* Interactor) const { return Title; }
bool ANGInventoryBox::CanInteract_Implementation(APawn* Interactor) const
{
	const auto* Interaction = IsValid(Interactor) ? Interactor->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	return Interaction && Interaction->IsCurrentInteractor(false) && IsValid(Inventory) && InteractionType.IsValid();
}
bool ANGInventoryBox::Interact_Implementation(APawn* Interactor)
{
	auto* Player = Interactor ? Interactor->GetPlayerState<APlayerState>() : nullptr;
	auto* Transport = Player ? Player->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
	if (!HasAuthority() || !Transport || !IsExecutingInteraction(Interactor) || !CanInteract_Implementation(Interactor)) return false;
	Transport->OpenInventorySource(Inventory, InteractionType);
	return true;
}
