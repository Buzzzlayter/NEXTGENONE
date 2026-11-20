#pragma once

#include "Gameplay/AI/States/NGAIActionState.h"
#include "Templates/SubclassOf.h"
#include "NGAIEquipItemState.generated.h"

class UNGInventoryItemDefinition;
class UNGInventoryItemInstance;

UCLASS(Blueprintable, DisplayName="AI Equip Item State", PrioritizeCategories=("AI|Equipment"))
class NEXTGENONE_API UNGAIEquipItemState : public UNGAIActionState
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Equipment", meta=(ExposeOnSpawn="true"))
	TSubclassOf<UNGInventoryItemDefinition> ItemClass;

protected:
	virtual void StartAction() override;
	virtual void ReleaseActionResources() override;

private:
	UFUNCTION() void HandleItemChanged();

	TWeakObjectPtr<UNGInventoryItemInstance> PendingItem;
};
