#pragma once

#include "Gameplay/AI/States/NGAIActionState.h"
#include "NGAITacticalDecisionState.generated.h"

enum class ENGAITacticalLane : uint8 { Unified, Mobility, Weapon };

UCLASS(Blueprintable)
class NEXTGENONE_API UNGAITacticalDecisionState : public UNGAIActionState
{
	GENERATED_BODY()

public:
	virtual TArray<FName> GetDeclaredTransitionEvents() const override;

protected:
	virtual void StartAction() override;
	virtual ENGAITacticalLane GetLane() const { return ENGAITacticalLane::Unified; }
};

UCLASS(Blueprintable, meta = (DisplayName = "AI Tactical Mobility Decision"))
class NEXTGENONE_API UNGAITacticalMobilityDecisionState : public UNGAITacticalDecisionState
{
	GENERATED_BODY()

public:
	virtual TArray<FName> GetDeclaredTransitionEvents() const override;

protected:
	virtual ENGAITacticalLane GetLane() const override { return ENGAITacticalLane::Mobility; }
};

UCLASS(Blueprintable, meta = (DisplayName = "AI Tactical Weapon Decision"))
class NEXTGENONE_API UNGAITacticalWeaponDecisionState : public UNGAITacticalDecisionState
{
	GENERATED_BODY()

public:
	virtual TArray<FName> GetDeclaredTransitionEvents() const override;

protected:
	virtual ENGAITacticalLane GetLane() const override { return ENGAITacticalLane::Weapon; }
};
