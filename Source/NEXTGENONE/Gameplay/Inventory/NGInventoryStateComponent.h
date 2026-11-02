#pragma once

#include "CoreMinimal.h"
#include "Components/HFSMPlayerStateComponent.h"
#include "Components/UIWidgetMounterComponent.h"
#include "Input/InputData.h"
#include "NGInventoryStateComponent.generated.h"

class ANextgenonePlayerController;
class UNGInventoryManagerComponent;
class UInputAction;
class UPlayerInputSubsystem;
class UEnhancedInputComponent;

UCLASS()
class UNGInventoryWindowMounter : public UUIWidgetMounterComponent
{
	GENERATED_BODY()
public:
	void Configure(TSubclassOf<UUserWidget> Class);
};

/** Inventory window belongs to the existing GameRuntime HFSM lifetime and UIWidgetMounter. */
UCLASS(Blueprintable, EditInlineNew, DisplayName="Inventory Window")
class NEXTGENONE_API UNGInventoryStateComponent : public UHFSMPlayerStateComponent
{
	GENERATED_BODY()
public:
	virtual bool Enter() override;
	virtual void Exit(FName Event) override;
	virtual void AbortEnter() override;
	UPROPERTY(EditAnywhere, Category="Inventory") TSoftClassPtr<UUserWidget> PanelClass;
	UPROPERTY(EditAnywhere, Category="Inventory|Input") TObjectPtr<UInputAction> ToggleAction;
private:
	TWeakObjectPtr<ANextgenonePlayerController> Controller;
	UPROPERTY(Transient) TObjectPtr<UNGInventoryWindowMounter> Mounter;
	TWeakObjectPtr<UPlayerInputSubsystem> InputSubsystem;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	uint32 ToggleBinding = 0;
	FGuid PolicyHandle;
	void RequestToggle();
	UFUNCTION() void Toggle(UNGInventoryManagerComponent* Inventory);
	void Close();
	void Unbind();
};
