#pragma once

#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "NGEquipmentInstance.generated.h"

class UNGEquipmentComponent;
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
	bool IsCurrent() const;
	UNGEquipmentComponent* GetEquipment() const;
	APawn* GetAvatar() const;
	UNGInventoryItemInstance* GetItem() const { return Item; }
	UNGEquipmentDefinition* GetDefinition() const { return Definition; }
	FGameplayTag GetSlot() const { return Slot; }
	int32 GetGeneration() const { return Generation; }
private:
	friend class UNGEquipmentComponent;
	// Local startup attempts belong to this installation and expire with it.
	TSet<FGameplayAbilitySpecHandle> StartupAttempted;
	UPROPERTY(ReplicatedUsing=OnRep_Source) TObjectPtr<UNGEquipmentDefinition> Definition;
	UPROPERTY(ReplicatedUsing=OnRep_Source) FGameplayTag Slot;
	UPROPERTY(ReplicatedUsing=OnRep_Source) int32 Generation = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Source) TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	UPROPERTY(ReplicatedUsing=OnRep_Source) bool bInstalled = false;
	UPROPERTY(ReplicatedUsing=OnRep_Source) TObjectPtr<UNGInventoryItemInstance> Item;
	UFUNCTION() void OnRep_Source();
};
