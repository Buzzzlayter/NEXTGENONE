#pragma once

#include "HFSMStateComponent.h"
#include "GameplayTagContainer.h"
#include "NGInteractionStateComponent.generated.h"

class UNGInteractionComponent;
class APawn;
class APlayerController;

/** UI graph routing only. Does not grant access, own inventory or add input bindings. */
UCLASS(EditInlineNew, meta=(DisplayName="Interaction UI Router"))
class NEXTGENONE_API UNGInteractionStateComponent : public UHFSMStateComponent
{
	GENERATED_BODY()
public:
	UNGInteractionStateComponent();
	/** Exact interaction type -> event in the owning UI graph. Configure graph names here. */
	UPROPERTY(EditAnywhere, Category="Interaction") TMap<FGameplayTag, FName> Routes;
	UPROPERTY(EditAnywhere, Category="Interaction") FName ClosedEvent;
	virtual void Enter() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Exit(FName Event) override;
private:
	void BindPawn(APawn* NewPawn);
	void Route();
	UFUNCTION() void PawnChanged(APawn* OldPawn, APawn* NewPawn);
	TWeakObjectPtr<UNGInteractionComponent> ObservedInteraction;
	TWeakObjectPtr<APlayerController> ObservedController;
	bool bActive = false;
	bool bClosedSent = false;
};

/** Place on the parent Interaction state; child mounters never terminate the session. */
UCLASS(EditInlineNew, meta=(DisplayName="Interaction Session"))
class NEXTGENONE_API UNGInteractionSessionStateComponent : public UNGInteractionStateComponent
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
private:
	TWeakObjectPtr<UNGInteractionComponent> SessionInteraction;
};
