#pragma once

#include "CoreMinimal.h"
#include "HFSMStateComponent.generated.h"

class UHFSMState;

UCLASS(Blueprintable, EditInlineNew)
class UIMODULE_API UHFSMStateComponent : public UObject
{
	GENERATED_BODY()
	
public:
	void Init(UHFSMState* StateOwner);

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
	
	virtual UWorld* GetWorld() const override;
	virtual void PostInitProperties() override;
	virtual bool IsTickEnabled();

protected:
	UPROPERTY(EditAnywhere)
	bool bIsTickEnabled = false;
	
	UPROPERTY(BlueprintReadOnly)
	UHFSMState* State;
	
	UPROPERTY(BlueprintReadOnly)
	bool bShouldInit = true;
	
	UPROPERTY(BlueprintReadOnly)
	UWorld* World;
};
