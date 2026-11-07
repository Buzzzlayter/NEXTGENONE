#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/Interaction/NGInteractionContext.h"
#include "NGInteraction.generated.h"

class APawn;
class ANGItemPickup;
inline constexpr ECollisionChannel NGInteractionChannel=ECC_GameTraceChannel1;

UINTERFACE(BlueprintType)
class NEXTGENONE_API UNGInteractable : public UInterface { GENERATED_BODY() };

class NEXTGENONE_API INGInteractable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction") FText GetInteractionPrompt(APawn* Interactor) const;
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction") bool CanInteract(APawn* Interactor) const;
	// Mutation runs on authority, inside the target actor's validated interaction scope.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction") bool Interact(APawn* Interactor);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction") void SetInteractionFocus(APawn* Interactor, bool bFocused);
};

/** An interactable owns its synchronous execution guard; no world registry is required. */
UCLASS(Abstract, Blueprintable)
class NEXTGENONE_API ANGInteractableActor : public AActor, public INGInteractable
{
	GENERATED_BODY()
public:
	bool ExecuteInteraction(APawn* Pawn, TFunctionRef<bool()> Prepare);
	bool IsExecutingInteraction(const APawn* Pawn) const;
private:
	bool bInteractionInProgress = false;
	TWeakObjectPtr<APawn> ExecutingInteractor;
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
	bool IsCurrentInteractor(bool bAuthority=true) const;
	AActor* FindTarget() const;
	bool ValidateTarget(AActor* Target) const;
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
	UFUNCTION(Server, Reliable) void ServerRequestInteract(APawn* ExpectedPawn,AActor* ProposedTarget);
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
