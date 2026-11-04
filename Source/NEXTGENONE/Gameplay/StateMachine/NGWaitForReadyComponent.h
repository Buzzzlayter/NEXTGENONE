#pragma once

#include "HFSMStateComponent.h"
#include "GameplayTagContainer.h"
#include "NGWaitForReadyComponent.generated.h"

/** Local UI gate. Add to WaitForReady; does not initialize or possess the player. */
UCLASS(Blueprintable, EditInlineNew, meta=(DisplayName="Wait For Player Ready"))
class NEXTGENONE_API UNGWaitForReadyComponent : public UHFSMStateComponent
{
	GENERATED_BODY()
public:
	UNGWaitForReadyComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readiness") FName ReadyEvent = TEXT("Ready");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readiness") FGameplayTag RequiredCharacterState;
	/** Current missing dependency, useful in a loading screen or Blueprint debugger. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Readiness") FName WaitingFor;
	virtual void Enter() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Exit(FName Event) override;
	virtual bool IsTickEnabled() override { return bWaiting; }
private:
	FName FindMissingDependency() const;
	bool bWaiting = false;
};
