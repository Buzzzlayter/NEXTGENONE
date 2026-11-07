#pragma once

#include "CoreMinimal.h"
#include "HFSMStateComponent.generated.h"

class UHFSMState;
class UInputComponent;
class APlayerController;
class ULocalPlayer;

UCLASS(Blueprintable, EditInlineNew)
class UIMODULE_API UHFSMStateComponent : public UObject
{
	GENERATED_BODY()
	
public:
	virtual void Init(UHFSMState* StateOwner);

	// Lifecycle hooks; each component validates and cleans up its own resources.
	virtual void Enter();
	virtual void Tick(float DeltaTime);
	virtual void Exit(FName Event);

	UFUNCTION(BlueprintImplementableEvent)
	void OnEnter();

	UFUNCTION(BlueprintImplementableEvent)
	void OnTick(float DeltaTime);

	UFUNCTION(BlueprintImplementableEvent)
	void OnExit(FName Event);

	UFUNCTION(BlueprintCallable)
	UHFSMState* GetState() const;

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	UObject* GetContextOwner() const;
	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	APlayerController* GetPlayerController() const;
	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	ULocalPlayer* GetLocalPlayer() const;
	
	virtual UWorld* GetWorld() const override;
	virtual bool IsTickEnabled();

protected:
	void ResetBlueprintInput();
	virtual void BeginDestroy() override;
	UPROPERTY(EditDefaultsOnly)
	bool bIsTickEnabled = false;
	
	UPROPERTY(BlueprintReadOnly)
	UHFSMState* State;
	
	UPROPERTY(BlueprintReadOnly)
	bool bShouldInit = true;
	
	UPROPERTY(Transient, BlueprintReadOnly)
	UWorld* World;

private:
	friend class UHFSMState;
	UPROPERTY(Transient) TObjectPtr<UInputComponent> BlueprintInputComponent;

};
