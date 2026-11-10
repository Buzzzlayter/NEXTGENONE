#pragma once

#include "Components/ActorComponent.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "NGEquipmentComponent.generated.h"

class UNGEquipmentInstance;
class USceneComponent;
class USkeletalMeshComponent;
class UNGEquipmentDefinition;
class UNGInventoryItemInstance;
class UNGPlayerInventoryManagerComponent;
struct FNGEquipmentAbilityGrant;

USTRUCT()
struct FNGEquipmentResources
{
	GENERATED_BODY()
	UPROPERTY() FNGGrantedHandles Handles;
	UPROPERTY() TArray<FGameplayAbilitySpecHandle> PendingRequiredStartup;
	UPROPERTY() TObjectPtr<UAbilitySystemComponent> GrantedASC;
};

/** Avatar lifetime application of inventory equipment. Never owns item placement or ammo. */
UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGEquipmentComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* Flags) override;
	virtual void ReadyForReplication() override;
	/** The owning Avatar supplies its context; binding alone does not enable gameplay resources. */
	void BindContext(UAbilitySystemComponent* ASC, UNGPlayerInventoryManagerComponent* InInventory, USkeletalMeshComponent* InAttachmentMesh);
	/** Called by the Avatar coordinator after its gameplay resources are ready. */
	void ActivateEquipment();
	void Shutdown();
	void RequestEquip(bool bEquip);
	void UnequipHands();
	bool EquipHands(UNGInventoryItemInstance* Item);
	bool PreflightPlacement(UNGInventoryItemInstance* Item, int32 SlotIndex, FString& Error) const;
	bool IsCurrent(const UNGEquipmentInstance* Instance) const;
	UNGEquipmentInstance* FindInstance(FGameplayTag Slot) const;
	UNGInventoryItemInstance* GetSelectedItem() const { return SelectedItem; }
	int32 GetHandsRevision() const { return HandsRevision; }
	void RefreshReplicatedState();
	FSimpleMulticastDelegate OnEquipmentChanged;
	static UNGEquipmentDefinition* GetDefinition(const UNGInventoryItemInstance* Item);
private:
	UPROPERTY(ReplicatedUsing=OnRep_Equipment) TArray<TObjectPtr<UNGEquipmentInstance>> Installations;
	UPROPERTY(ReplicatedUsing=OnRep_Equipment) int32 HandsRevision = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Equipment) TObjectPtr<UNGInventoryItemInstance> SelectedItem;
	UPROPERTY() TMap<TObjectPtr<UNGEquipmentInstance>, FNGEquipmentResources> Resources;
	UPROPERTY() TMap<TObjectPtr<UNGEquipmentInstance>, TObjectPtr<USceneComponent>> Visuals;
	TWeakObjectPtr<UNGPlayerInventoryManagerComponent> Inventory;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	TWeakObjectPtr<USkeletalMeshComponent> AttachmentMesh;
	TSet<TWeakObjectPtr<UNGInventoryItemInstance>> FailedItems;
	FDelegateHandle EffectAddedHandle;
	FDelegateHandle AbilityEndedHandle;
	FTimerHandle ReadinessTimer;
	bool bChanging = false;
	bool bReconcilePending = false;
	bool bSuspended = true;
	bool bEnding = false;
	int32 NextGeneration = 0;
	UFUNCTION(Server, Reliable) void ServerUnequipHands();
	UFUNCTION() void OnRep_Equipment();
	UFUNCTION() void InventoryChanged();
	bool CanApply() const;
	bool Preflight(UNGInventoryItemInstance* Item, UNGEquipmentDefinition* Definition, FString& Error) const;
	bool Install(UNGInventoryItemInstance* Item, UNGEquipmentDefinition* Definition);
	bool FinishInstall(UNGEquipmentInstance* Instance);
	UNGEquipmentInstance* FindInstallation(FGameplayTag Slot) const;
	void Remove(UNGEquipmentInstance* Instance);
	void Reconcile();
	void Publish();
	void RefreshVisuals();
	void QueueDeferredUpdate();
	void ProcessDeferredUpdate();
	void AbilityEnded(const FAbilityEndedData& Data);
	void EffectAdded(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
};
