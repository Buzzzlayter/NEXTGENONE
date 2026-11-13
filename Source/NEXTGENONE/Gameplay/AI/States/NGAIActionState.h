#pragma once

#include "HFSMState.h"
#include "NGAIActionState.generated.h"

class ANGAIController;
class UNGAbilitySystemComponent;
class APawn;

enum class ENGAIActionResult : uint8 { Running, Succeeded, Failed, Aborted };

/** One state owns one action. No planning or gameplay rules live here. */
UCLASS(Abstract, Blueprintable)
class NEXTGENONE_API UNGAIActionState : public UHFSMState
{
	GENERATED_BODY()
public:
	virtual TArray<FName> GetDeclaredTransitionEvents() const override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Action", meta=(ExposeOnSpawn="true", ClampMin="0.1")) float ActionTimeout = 10.f;
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
	ENGAIActionResult GetResult() const { return Result; }
	FName GetResultReason() const { return ResultReason; }
protected:
	/** Assigned by GCFSM before Enter; also used for editor context validation. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="AI|Context")
	TObjectPtr<ANGAIController> Context;

	virtual void StartAction() PURE_VIRTUAL(UNGAIActionState::StartAction, );
	virtual void ReleaseActionResources() {}
	void FinishAction(ENGAIActionResult Outcome, FName Reason);
	bool IsCurrentRun(uint64 Serial) const;
	bool HasCurrentAvatar() const;
	TWeakObjectPtr<ANGAIController> ActionController;
	TWeakObjectPtr<APawn> ExpectedPawn;
	TWeakObjectPtr<UNGAbilitySystemComponent> ActionASC;
	uint64 RunSerial = 0;
	bool bRunning = false;
private:
	/** Release owned work without scheduling a result or a graph transition. */
	void ResetAction();
	uint32 AvatarGeneration = 0;
	ENGAIActionResult Result = ENGAIActionResult::Aborted;
	FName ResultReason;
	FTimerHandle TimeoutTimer;
	FTimerHandle CompletionTimer;
};
