#pragma once
#include "CoreMinimal.h"
#include "Character/ALSPlayerController.h"
#include "NextgenonePlayerController.generated.h"

class UNGInventoryManagerComponent;
class UNGInventoryItemInstance;
class ANextgenoneCharacter;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGInventoryUIRequested, UNGInventoryManagerComponent*, Inventory);
UCLASS()
class NEXTGENONE_API ANextgenonePlayerController : public AALSPlayerController
{
    GENERATED_BODY()
public:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void AddYawInput(float Value) override;
    virtual void AddPitchInput(float Value) override;
    virtual void OnPossess(APawn* ControlledPawn) override;
    virtual void OnRep_Pawn() override;
    // Compatibility for existing UI graphs; FSM owns flags, subsystem owns input blocking.
    UPROPERTY(BlueprintReadOnly, Category="UI") bool bSettingsUIOpen = false;
    UPROPERTY(BlueprintReadOnly, Category="Inventory|UI") bool bInventoryUIOpen = false;
    UFUNCTION(BlueprintImplementableEvent, Category="Inventory|UI") void OnInventoryRequested(UNGInventoryManagerComponent* Inventory);
    UPROPERTY(BlueprintAssignable, Category="Inventory|UI") FNGInventoryUIRequested InventoryUIRequested;
    UFUNCTION(BlueprintCallable, Category="Inventory|UI") void ToggleInventoryUI();
    UFUNCTION(BlueprintCallable, Category="Inventory") void RequestDropItem(UNGInventoryItemInstance* Item, int32 Count=0);
    UFUNCTION(Server, Reliable) void ServerDropItem(ANextgenoneCharacter* ExpectedPawn, UNGInventoryItemInstance* Item, int32 Count);
    // Existing test transport remains opt-in; no input bindings or tests are installed here.
    UFUNCTION(Client, Reliable) void ClientSmoke(int32 Command);
    UFUNCTION(Server, Reliable) void ServerSmokeResult(int32 Command, bool bPassed);
private:
	TWeakObjectPtr<APawn> PreviousInputPawn;
    void RefreshPawnPresentation();
};
