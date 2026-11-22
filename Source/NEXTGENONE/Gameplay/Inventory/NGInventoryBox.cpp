#include "NGInventoryBox.h"
#include "NativeGameplayTags.h"

#include "GameFramework/Pawn.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "NGPlayerInventoryManagerComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Serialization/Archive.h"
#include "Serialization/CustomVersion.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGInteractionStorage, "Interaction.Storage");

namespace
{
const FGuid InventoryBoxVersion(0x9FC9CEB2, 0x99064223, 0x91A7EC96, 0x801A0145);
constexpr int32 IndependentCollisionRoot = 1;
const FCustomVersionRegistration RegisterInventoryBoxVersion(InventoryBoxVersion, IndependentCollisionRoot, TEXT("NGInventoryBox"));
}

ANGInventoryBox::ANGInventoryBox()
{
	bReplicates = true;
	InteractionType = TAG_NGInteractionStorage;
	BoxRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoxRoot"));
	SetRootComponent(BoxRoot);
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(BoxRoot);
	Collision->SetBoxExtent(FVector(40.f));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(ECC_WorldStatic);
	Collision->SetCollisionResponseToChannel(NGInteractionChannel, ECR_Block);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(BoxRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Inventory = CreateDefaultSubobject<UNGInventoryManagerComponent>(TEXT("Inventory"));
	Inventory->bOwnerOnly = false;
}

void ANGInventoryBox::Serialize(FArchive& Ar)
{
	if (Ar.IsPersistent())
	{
		Ar.UsingCustomVersion(InventoryBoxVersion);
		if (Ar.IsLoading())
		{
			bNeedsCollisionRootMigration = Ar.CustomVer(InventoryBoxVersion) < IndependentCollisionRoot;
		}
	}
	Super::Serialize(Ar);
}

void ANGInventoryBox::PostLoad()
{
	Super::PostLoad();
	if (!bNeedsCollisionRootMigration || !BoxRoot || !Collision
		|| (GetRootComponent() != BoxRoot && GetRootComponent() != Collision))
	{
		return;
	}
	bNeedsCollisionRootMigration = false;

	const bool bCollisionWasRoot = GetRootComponent() == Collision;
	const FTransform PreviousTransform = Collision->GetRelativeTransform();
	if (!SetRootComponent(BoxRoot))
	{
		return;
	}
	if (bCollisionWasRoot || !PreviousTransform.Equals(FTransform::Identity))
	{
		BoxRoot->SetRelativeTransform_Direct(PreviousTransform);
	}
	if (USceneComponent* PreviousParent = Collision->GetAttachParent(); PreviousParent && PreviousParent->GetOwner() != this)
	{
		BoxRoot->SetupAttachment(PreviousParent, Collision->GetAttachSocketName());
	}
	BoxRoot->SetUsingAbsoluteLocation(Collision->IsUsingAbsoluteLocation());
	BoxRoot->SetUsingAbsoluteRotation(Collision->IsUsingAbsoluteRotation());
	BoxRoot->SetUsingAbsoluteScale(Collision->IsUsingAbsoluteScale());
	Collision->SetUsingAbsoluteLocation(false);
	Collision->SetUsingAbsoluteRotation(false);
	Collision->SetUsingAbsoluteScale(false);
	Collision->SetRelativeTransform_Direct(FTransform::Identity);
	Collision->SetupAttachment(BoxRoot);
	if (Mesh && Mesh->GetAttachParent() == Collision)
	{
		Mesh->SetupAttachment(BoxRoot, Mesh->GetAttachSocketName());
	}
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
