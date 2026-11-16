#pragma once

#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "NGEquipmentInstance.generated.h"

class UNGEquipmentDefinition;
class UNGInventoryItemInstance;
class APawn;

/** A fresh GAS source for each installation, never reused by a later equip or life. */
UCLASS(BlueprintType)
class NEXTGENONE_API UNGEquipmentInstance : public UObject
{
	GENERATED_BODY()
public:
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	APawn* GetAvatar() const;
	UNGInventoryItemInstance* GetItem() const { return Item; }
	UNGEquipmentDefinition* GetDefinition() const { return Definition; }
	FGameplayTag GetSlot() const { return Slot; }
	int32 GetGeneration() const { return Generation; }
	FSimpleMulticastDelegate OnSourceReplicated;
	void Initialize(UNGInventoryItemInstance* InItem, UNGEquipmentDefinition* InDefinition, int32 InGeneration);
private:
	UPROPERTY(ReplicatedUsing=OnRep_Source) TObjectPtr<UNGEquipmentDefinition> Definition;
	UPROPERTY(ReplicatedUsing=OnRep_Source) FGameplayTag Slot;
	UPROPERTY(ReplicatedUsing=OnRep_Source) int32 Generation = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Source) TObjectPtr<UNGInventoryItemInstance> Item;
	UFUNCTION() void OnRep_Source();
};
