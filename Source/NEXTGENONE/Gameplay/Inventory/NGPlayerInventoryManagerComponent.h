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

UENUM(BlueprintType)
enum class ENGInventoryRequestResult : uint8
{
    Succeeded, InvalidRequest, StalePawn, RateLimited, AccessDenied, StaleItem, Rejected
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FNGInventoryRequestCompleted, uint32, ENGInventoryRequestResult);

/** PlayerState-owned storage, player requests and external-container session. */
UCLASS(Blueprintable, ClassGroup=Inventory, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGPlayerInventoryManagerComponent : public UNGInventoryManagerComponent
{
    GENERATED_BODY()
public:
    UNGPlayerInventoryManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, Category="Inventory|Starter") TArray<FNGInventoryStarterEntry> InitialContents;
	/** Serialized for manual migration only; runtime grants use InitialContents. */
	UPROPERTY(EditDefaultsOnly, Category="Inventory|Legacy", meta=(DisplayName="Legacy Starter Items (Migration Only)")) TArray<TSoftClassPtr<UNGInventoryItemDefinition>> StarterItems;
	void GrantStarterInventory();
	UNGInventoryManagerComponent* GetViewedInventory() const;
	void OpenInventorySource(UNGInventoryManagerComponent* Source, FGameplayTag InteractionType);
	void CloseInventoryView();
	/** Nonzero ID means submitted, not accepted. Completion does not imply replication has arrived. */
	uint32 RequestInventoryTransfer(UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount=0, FNGInventoryRequestCompleted::FDelegate Completion = {});
	FNGInventoryRequestCompleted OnRequestCompleted;
	/** Forget UI callbacks only; an already submitted server operation is not cancelled. */
	void ForgetRequestCallbacks(const UObject* Listener);
	UPROPERTY(EditDefaultsOnly, Category="Inventory", meta=(ClampMin="0.01")) float InventoryOperationInterval = .05f;
    uint32 RequestDropItem(UNGInventoryItemInstance* Item, int32 Count=0, FNGInventoryRequestCompleted::FDelegate Completion = {});
    UFUNCTION(Server, Reliable) void ServerDropItem(uint32 RequestId, ANextgenoneCharacter* ExpectedPawn, UNGInventoryItemInstance* Item, int32 Count);

private:
	bool TryBeginStarterGrant();
	uint32 LastRequestId = 0;
	TMap<uint32, FNGInventoryRequestCompleted::FDelegate> RequestCallbacks;
	uint32 AllocateRequestId();
	UFUNCTION(Client, Reliable) void ClientRequestCompleted(uint32 RequestId, ENGInventoryRequestResult Result);
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
	UFUNCTION(Server, Reliable) void ServerInventoryTransfer(uint32 RequestId, ANextgenoneCharacter* ExpectedPawn, UNGInventoryManagerComponent* Source, UNGInventoryItemInstance* ExpectedItem, int32 SourceSlot, UNGInventoryManagerComponent* Target, int32 TargetSlot, UNGInventoryItemInstance* ExpectedTarget, int32 Amount);

	APlayerController* GetPlayerController() const;
	APawn* GetInventoryPawn() const;
	bool IsLocalController() const;

    UFUNCTION() void PawnChanged(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);
};
