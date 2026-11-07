#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "NGInventoryBox.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** World storage using exactly the same inventory component as a player or bag. */
UCLASS(Blueprintable)
class NEXTGENONE_API ANGInventoryBox : public ANGInteractableActor
{
	GENERATED_BODY()
public:
	ANGInventoryBox();
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UNGInventoryManagerComponent> Inventory;
	/** Optional server-authored seed. A backend can instead call Inventory.ImportItemState. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory") TArray<FNGInventorySlotState> InitialContents;
	UPROPERTY(EditAnywhere, Category="Interaction") FGameplayTag InteractionType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory") FText Title = NSLOCTEXT("NEXTGENONE", "InventoryBoxTitle", "Storage");
	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor) const override;
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual bool Interact_Implementation(APawn* Interactor) override;
};
