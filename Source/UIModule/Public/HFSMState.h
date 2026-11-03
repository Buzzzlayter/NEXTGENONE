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
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "ComponentClass", DynamicOutputParam = "OutComponent"))
	bool GetClosestComponentInHierarchyUp(
		TSubclassOf<UHFSMStateComponent> ComponentClass,
		UHFSMStateComponent*& OutComponent);

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	UObject* GetContextOwner() const { return GetContextObject(); }
	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	APlayerController* GetPlayerController() const;
	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	ULocalPlayer* GetLocalPlayer() const;
	
protected:
	virtual void BeginDestroy() override;
	virtual void Enter() override;
	virtual void Tick(float deltaTime) override;
	virtual void Exit(FName event) override;

private:
	friend class FHFSMComponentActivationTest;

	UPROPERTY(EditAnywhere, Instanced)
	TArray<UHFSMStateComponent*> Components;

	UPROPERTY(Transient) TObjectPtr<UInputComponent> BlueprintInputComponent;

	UHFSMState* GetRoot();
	UHFSMState* GetParent() const;
	TArray<UHFSMState*> GetChildren() const;
	
	void IterateUpConditional(TFunctionRef<bool(UHFSMState*)> Func);
	void IterateDownConditional(TFunctionRef<bool(UHFSMState*)> Func);
};
