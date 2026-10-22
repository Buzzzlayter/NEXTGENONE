#pragma once

#include "CoreMinimal.h"
#include "HFSMStateComponent.generated.h"

class UHFSMState;

UCLASS(Blueprintable, EditInlineNew)
class UIMODULE_API UHFSMStateComponent : public UObject
{
	GENERATED_BODY()
	
public:
	virtual void Init(UHFSMState* StateOwner);

	// A failed entry must release its partial resources before returning false.
	virtual bool Enter();
	virtual void Tick(float DeltaTime);
	virtual void Exit(FName Event);
	// Roll back a successful entry when another component fails to enter.
	virtual void AbortEnter();

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
	
	virtual UWorld* GetWorld() const override;
	virtual bool IsTickEnabled();

protected:
	UPROPERTY(EditAnywhere)
	bool bIsTickEnabled = false;
	
	UPROPERTY(BlueprintReadOnly)
	UHFSMState* State;
	
	UPROPERTY(BlueprintReadOnly)
	bool bShouldInit = true;
	
	UPROPERTY(Transient, BlueprintReadOnly)
	UWorld* World;

};
