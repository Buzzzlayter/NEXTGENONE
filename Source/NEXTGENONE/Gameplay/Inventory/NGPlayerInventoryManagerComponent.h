#pragma once

#include "Inventory/NGInventoryManagerComponent.h"
#include "NGPlayerInventoryManagerComponent.generated.h"

class APawn;
class APlayerState;
class APlayerController;
class ANextgenoneCharacter;
class UNGInventoryManagerComponent;
class UNGInventoryItemInstance;
class UNGInteractionComponent;


/** PlayerState-owned storage, player requests and external-container session. */
UCLASS(Blueprintable, ClassGroup=Inventory, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGPlayerInventoryManagerComponent : public UNGInventoryManagerComponent
{
    GENERATED_BODY()
public:
    UNGPlayerInventoryManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
	bool TryBeginStarterGrant();
	UNGInventoryManagerComponent* GetViewedInventory() const;
	void OpenInventorySource(UNGInventoryManagerComponent* Source, FGameplayTag InteractionType);
	void CloseInventoryView();
	void RequestInventoryTransfer(UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount=0);
	UPROPERTY(EditDefaultsOnly, Category="Inventory", meta=(ClampMin="0.01")) float InventoryOperationInterval = .05f;
    void RequestDropItem(UNGInventoryItemInstance* Item, int32 Count=0);
    UFUNCTION(Server, Reliable) void ServerDropItem(ANextgenoneCharacter* ExpectedPawn, UNGInventoryItemInstance* Item, int32 Count);

private:
	double NextDropTime = 0.0;
	double NextOperationTime = 0.0;
	bool bStarterGrantStarted = false;
	TWeakObjectPtr<UNGInventoryManagerComponent> ViewedInventory;
	TWeakObjectPtr<APawn> ViewedPawn;
	TWeakObjectPtr<UNGInteractionComponent> SessionInteraction;
	void BindInteractionSession(APawn* Pawn);
	void InteractionEnded(UObject* Source);
	void ClearViewedContext();
	bool CanAccessInventory(UNGInventoryManagerComponent* Inventory) const;
	UFUNCTION(Client, Reliable) void ClientShowInventory(ANextgenoneCharacter* ExpectedPawn, UNGInventoryManagerComponent* Source, FGameplayTag InteractionType);
	UFUNCTION(Server, Reliable) void ServerCloseInventoryView(APawn* ExpectedPawn, UNGInventoryManagerComponent* ExpectedSource);
	UFUNCTION(Client, Reliable) void ClientCloseInventoryView(APawn* ExpectedPawn, UNGInventoryManagerComponent* ExpectedSource);
	UFUNCTION(Server, Reliable) void ServerInventoryTransfer(ANextgenoneCharacter* ExpectedPawn, UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount);

	APlayerController* GetPlayerController() const;
	APawn* GetInventoryPawn() const;
	bool IsLocalController() const;

    UFUNCTION() void PawnChanged(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);
};
