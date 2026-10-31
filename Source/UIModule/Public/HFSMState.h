#pragma once

#include "CoreMinimal.h"
#include "GCFSMState.h"
#include "HFSMStateComponent.h"
#include "Templates/Function.h"
#include "UObject/NoExportTypes.h"
#include "HFSMState.generated.h"

UCLASS(Blueprintable)
class UIMODULE_API UHFSMState : public UGCFSMState
{
	GENERATED_BODY()
	
public:
	UHFSMStateComponent* GetComponent(TSubclassOf<UHFSMStateComponent> ComponentClass) const;

	TArray<UHFSMStateComponent*> GetComponents(TSubclassOf<UHFSMStateComponent> ComponentClass) const;
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "ComponentClass", DynamicOutputParam = "OutComponent"))
	bool GetClosestComponentInHierarchyUp(
		TSubclassOf<UHFSMStateComponent> ComponentClass,
		UHFSMStateComponent*& OutComponent);

	UFUNCTION(BlueprintPure, Category = "HFSM")
	bool HasEnteredSuccessfully() const { return bEnteredSuccessfully; }

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	UObject* GetContextOwner() const { return GetContextObject(); }
	
protected:
	virtual void Enter() override;
	virtual void Tick(float deltaTime) override;
	virtual void Exit(FName event) override;

	UFUNCTION(BlueprintImplementableEvent)
	void LaunchChildFSMs();

	UFUNCTION(BlueprintImplementableEvent, Category = "HFSM")
	void OnEnterFailed(UHFSMStateComponent* FailedComponent);

private:
	friend class FHFSMComponentActivationTest;

	UPROPERTY(EditAnywhere, Instanced)
	TArray<UHFSMStateComponent*> Components;

	UPROPERTY(Transient)
	TArray<UHFSMStateComponent*> ActiveComponents;

	bool bEnteredSuccessfully = false;

	UHFSMState* GetRoot();
	UHFSMState* GetParent() const;
	TArray<UHFSMState*> GetChildren();
	TArray<UHFSMStateComponent*> GetComponents();

	void IterateUpConditional(TFunctionRef<bool(UHFSMState*)> Func);
	void IterateDownConditional(TFunctionRef<bool(UHFSMState*)> Func);
};
