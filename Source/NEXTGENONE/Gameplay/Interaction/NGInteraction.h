#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/Interaction/NGInteractionContext.h"
#include "NGInteraction.generated.h"

class ANextgenoneCharacter;
class ANGItemPickup;
class UNGInventoryItemInstance;
class UStaticMeshComponent;
inline constexpr ECollisionChannel NGInteractionChannel=ECC_GameTraceChannel1;

UINTERFACE(BlueprintType)
class NEXTGENONE_API UNGInteractable : public UInterface { GENERATED_BODY() };

class NEXTGENONE_API INGInteractable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction") FText GetInteractionPrompt(ANextgenoneCharacter* Interactor) const;
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction") bool CanInteract(ANextgenoneCharacter* Interactor) const;
	// Mutation is called by the subsystem on authority, inside its validated interaction scope.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction") bool Interact(ANextgenoneCharacter* Interactor);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction") void SetInteractionFocus(ANextgenoneCharacter* Interactor, bool bFocused);
};

UCLASS(BlueprintType)
class NEXTGENONE_API UNGInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Interaction") AActor* FindTarget(ANextgenoneCharacter* Interactor, float MaxDistance=350.f, float CaptureRadius=45.f) const;
	bool IsCurrentInteractor(const ANextgenoneCharacter* Pawn, bool bAuthority=true) const;
	bool ValidateTarget(ANextgenoneCharacter* Pawn,AActor* Target,float MaxDistance) const;
	bool TryInteract(ANextgenoneCharacter* Pawn, AActor* Target, float MaxDistance, TFunctionRef<bool()> Commit);
	bool IsExecutingInteraction(const AActor* Target, const ANextgenoneCharacter* Pawn) const;
	bool TakePickup(ANGItemPickup* Pickup, ANextgenoneCharacter* Pawn);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory|World")
	ANGItemPickup* DropInventoryItem(ANextgenoneCharacter* Pawn, UNGInventoryItemInstance* Item, int32 Count=0);
private:
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<ANextgenoneCharacter>> ActiveInteractions;
	TSet<TWeakObjectPtr<AActor>> BusyActors;
	TSet<TWeakObjectPtr<ANGItemPickup>> ActivePickups;
	TSet<TWeakObjectPtr<UNGInventoryItemInstance>> ActiveDrops;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNGInteractionFocusChanged, AActor*, Target, FText, Prompt, bool, bAvailable);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGInteractionResult, bool, bSucceeded);

UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGInteractionComponent();
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& ValidationContext) const override;
#endif
	const FNGInteractionContext& GetContext() const { return Context; }
	void SetConfirmedContext(FGameplayTag Type, AActor* Target, UObject* Source);
	void ClearContext(UObject* ExpectedSource);
	void EndInteraction();
	void BindAbilitySystem(UAbilitySystemComponent* ASC);
	bool IsInteractionBlocked() const;
	FNGInteractionContextChanged OnContextChanged;
	FNGInteractionEnded OnInteractionEnded;
	UPROPERTY(EditDefaultsOnly, Category="Interaction|GAS") FGameplayTagContainer InterruptingTags;
	UPROPERTY(EditDefaultsOnly, Category="Inventory|World") TSubclassOf<ANGItemPickup> DroppedPickupClass;
	virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* ThisTick) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Interaction", meta=(ClampMin="50",ClampMax="500")) float MaxDistance=350.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Interaction", meta=(ClampMin="0",ClampMax="100")) float CaptureRadius=45.f;
	UPROPERTY(BlueprintAssignable, Category="Interaction") FNGInteractionFocusChanged OnFocusChanged;
	UPROPERTY(BlueprintAssignable, Category="Interaction") FNGInteractionResult OnInteractionResult;
	UFUNCTION(BlueprintPure, Category="Interaction") AActor* GetFocusedActor() const { return FocusedActor.Get(); }
	UFUNCTION(BlueprintPure, Category="Interaction") FText GetFocusedPrompt() const { return FocusedPrompt; }
	UFUNCTION(BlueprintPure, Category="Interaction") bool CanUseFocusedActor() const { return bFocusedAvailable; }
	UFUNCTION(BlueprintCallable, Category="Interaction") void RequestInteract();
	UFUNCTION(Server, Reliable) void ServerRequestInteract(ANextgenoneCharacter* ExpectedPawn,AActor* ProposedTarget);
	UFUNCTION(Client, Reliable) void ClientInteractionResult(bool bSuccess);

private:
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TMap<FGameplayTag, FDelegateHandle> TagHandles;
	bool bEndingInteraction = false;
	void BlockingTagChanged(FGameplayTag Tag, int32 Count);
	FNGInteractionContext Context;
	TWeakObjectPtr<AActor> FocusedActor;
	FText FocusedPrompt;
	bool bFocusedAvailable=false;
	void UpdateFocus(AActor* Target);
};

UCLASS(Blueprintable)
class NEXTGENONE_API UNGInteractionAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UNGInteractionAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
		FGameplayTagContainer* RelevantTags=nullptr) const override;
	// Kept for existing assets; current Characters use Interaction.MaxDistance as the single range setting.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Interaction", meta=(DeprecatedProperty,DeprecationMessage="Configure Character.Interaction.MaxDistance")) float MaxDistance=350.f;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGInteractiveStateChanged, bool, bActive);

/** Ready-to-subclass replicated interactive object; useful for switches and other non-inventory actions. */
UCLASS(Blueprintable)
class NEXTGENONE_API ANGInteractiveObject : public AActor, public INGInteractable
{
	GENERATED_BODY()
public:
	ANGInteractiveObject();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Interaction") bool bEnabled=true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") bool bOneShot=false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") FText InteractionText;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Active, Category="Interaction") bool bActive=false;
	UPROPERTY(BlueprintAssignable, Category="Interaction") FNGInteractiveStateChanged OnStateChanged;
	virtual FText GetInteractionPrompt_Implementation(ANextgenoneCharacter* Interactor) const override;
	virtual bool CanInteract_Implementation(ANextgenoneCharacter* Interactor) const override;
	virtual bool Interact_Implementation(ANextgenoneCharacter* Interactor) override;
	virtual void SetInteractionFocus_Implementation(ANextgenoneCharacter* Interactor, bool bFocused) override;
protected:
	UFUNCTION(BlueprintImplementableEvent, Category="Interaction") void OnInteractedOnServer(ANextgenoneCharacter* Interactor);
private:
	UFUNCTION() void OnRep_Active();
};
