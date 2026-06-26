#pragma once

#include "CoreMinimal.h"
#include "GCFSMState.h"
#include "HFSMStateComponent.h"
#include "UObject/NoExportTypes.h"
#include "HFSMState.generated.h"

UCLASS(Blueprintable)
class UIMODULE_API UHFSMState : public UGCFSMState
{
	GENERATED_BODY()
	
public:
	template<class T>
	T* GetComponent();

	template<class T>
	TArray<T*> GetComponents();
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "ComponentClass", DynamicOutputParam = "OutComponent"))
	bool GetClosestComponentInHierarchyUp(
		TSubclassOf<UHFSMStateComponent> ComponentClass,
		UHFSMStateComponent*& OutComponent);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (CompactNodeTitle = "MultiTransition"))
	static void MultiTransition(UObject* stateOrContext, TArray<FGameplayTag> Events);

	UFUNCTION(BlueprintCallable,BlueprintCosmetic, meta = (CompactNodeTitle = "SimpleTransition"))
	static void SimpleTransition(UObject* stateOrContext, FGameplayTag Event);
	
protected:
	virtual void Enter() override;
	virtual void Tick(float deltaTime) override;
	virtual void Exit(FName event) override;

	UFUNCTION(BlueprintImplementableEvent)
	void LaunchChildFSMs();

private:
	UPROPERTY(EditAnywhere, Instanced)
	TArray<UHFSMStateComponent*> Components;

	UHFSMState* GetRoot();
	UHFSMState* GetParent() const;
	TArray<UHFSMState*> GetChildren();
	TArray<UHFSMStateComponent*> GetComponents();

	
	template<typename TFunc>
	void IterateUpConditional(TFunc Func);

	template<typename TFunc>
	void IterateDownConditional(TFunc Func);
};


template<typename TFunc>
void UHFSMState::IterateUpConditional(TFunc Func)
{
	if (!Func(this))
		return;
	
	if (UHFSMState* Parent = GetParent())
	{
		Parent->IterateUpConditional(Func);
	}
}

template<typename TFunc>
void UHFSMState::IterateDownConditional(TFunc Func)
{
	if (!Func(this))
		return;

	TArray<UHFSMState*> Children = GetChildren();
	for (UHFSMState* Child : Children)
	{
		Child->IterateDownConditional(Func);
	}
}

template<class T>
T* UHFSMState::GetComponent()
{
	UHFSMStateComponent* const* Sample = Components.FindByPredicate(
		[](const UHFSMStateComponent* Component)
		{
			return Component->IsA<T>();
		});
	return Sample ? Cast<T>(*Sample) : nullptr;
}

template<class T>
TArray<T*> UHFSMState::GetComponents()
{
	TArray<T*> Output;
	for (UHFSMStateComponent* Component : Components)
	{
		if (Component && Component->IsA<T>())
		{
			Output.Add(Cast<T>(Component));
		}
	}
	return Output;
}
