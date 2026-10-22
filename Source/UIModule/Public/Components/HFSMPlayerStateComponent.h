#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "HFSMStateComponent.h"
#include "HFSMPlayerStateComponent.generated.h"

class UInputComponent;

UCLASS(Blueprintable, EditInlineNew)
class UIMODULE_API UHFSMPlayerStateComponent : public UHFSMStateComponent
{
	GENERATED_BODY()

public:
	virtual void Init(UHFSMState* StateOwner) override;
	virtual bool Enter() override;
	virtual void Exit(FName Event) override;

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	APlayerController* GetPlayerController() const;

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	ULocalPlayer* GetLocalPlayer() const;

private:
	void ReleasePlayerContext();
	FLocalPlayerContext PlayerContext;
	TWeakObjectPtr<UInputComponent> BoundInputComponent;
	bool bHasPlayerContext = false;
};
