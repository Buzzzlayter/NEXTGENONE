// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSM.h"
#include "GCFSMState.h"
#include "GCFSMSnapshot.h"
#include "Tickable.h"
#include "GCFSMRootState.generated.h"

UCLASS(NotBlueprintable, Hidden, DisplayName = "GC FSM Root State")
class GCFSM_API UGCFSMRootState final : public UGCFSMState
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	static UGCFSMRootState* MakeRootStateObject(UObject* context);

	UObject* GetBlueprintContext() override { return GetContextObject(); }

	void SetPaused(bool paused)
	{
		if (ticker.IsSet()) ticker->paused = paused;
	}

	bool IsPaused() const
	{
		return ticker.IsSet() && ticker->paused;
	}

	void SetTickableWhenGamePaused(bool tickable)
	{
		if (ticker.IsSet()) ticker->tickableWhenGamePaused = tickable;
	}

protected:
#if ENABLE_VISUAL_LOG
	FString MakeDetailedNameForLogging() const override;
#endif

private:
	void Initialize(UObject* context);

	struct FTicker : FTickableGameObject
	{
		UGCFSMRootState* state;
		bool paused;
		bool tickableWhenGamePaused;

		FTicker(UGCFSMRootState* state);
		void Tick(float deltaTime) override;
		ETickableTickType GetTickableTickType() const override	{ return ETickableTickType::Always; }
		bool IsTickableWhenPaused() const override				{ return tickableWhenGamePaused; }
		TStatId GetStatId() const override						{ return state->GetStatID(); }
		UWorld* GetTickableGameObjectWorld() const override		{ return state->GetWorld(); }
	};

	TOptional<FTicker> ticker;
};
