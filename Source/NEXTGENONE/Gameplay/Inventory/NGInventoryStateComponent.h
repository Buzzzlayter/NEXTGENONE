#pragma once

#include "CoreMinimal.h"
#include "Components/HFSMPlayerStateComponent.h"
#include "Components/UIWidgetMounterComponent.h"
#include "Input/InputData.h"
#include "NGInventoryStateComponent.generated.h"

class ANextgenonePlayerController;
class UNGInventoryManagerComponent;

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
private:
	TWeakObjectPtr<ANextgenonePlayerController> Controller;
	UPROPERTY(Transient) TObjectPtr<UNGInventoryWindowMounter> Mounter;
	EInputMode PreviousMode = EInputMode::Game;
	bool bPreviousCursor = false;
	UFUNCTION() void Toggle(UNGInventoryManagerComponent* Inventory);
	void Close();
	void Unbind();
};
