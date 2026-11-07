#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "NGItemPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;

UCLASS(Blueprintable)
class NEXTGENONE_API ANGItemPickup : public ANGInteractableActor
{
	GENERATED_BODY()
public:
	ANGItemPickup();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> InteractionCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> SkeletalMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Payload, Category="Item") TSubclassOf<UNGInventoryItemDefinition> ItemClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Payload, Category="Item", meta=(ClampMin="1")) int32 StackCount=1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") bool bAutoEquipWhenHandsEmpty=true;
	UFUNCTION(BlueprintPure, Category="Item") bool IsAvailable() const { return bAvailable; }
	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor) const override;
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual bool Interact_Implementation(APawn* Interactor) override;
	virtual void SetInteractionFocus_Implementation(APawn* Interactor, bool bFocused) override;
	void InitializeDroppedState(const FNGItemState& State);
	void PublishDrop();
	FNGItemState GetStateForTransfer() const;
	bool HasSavedItemState() const { return bHasSavedState; }
	int32 FindDestinationSlot(const APawn* Interactor) const;
	void ConsumeForTransfer();
private:
	bool bTransferring = false;
	UPROPERTY(ReplicatedUsing=OnRep_Available) bool bAvailable=true;
	UPROPERTY() FNGItemState SavedState;
	bool bHasSavedState=false;
	UFUNCTION() void OnRep_Payload();
	UFUNCTION() void OnRep_Available();
};
