// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMRootState.h"
#include "GCFSMModule.h"
#include "Blueprint/UserWidget.h"

UGCFSMRootState* UGCFSMRootState::MakeRootStateObject(UObject* context)
{
	check(context);
	// The outer of the new root state object has to be the context object itself, in order to:
	// 1) be destroyed when the context is destroyed and not any sooner
	// 2) allow the editor to properly debug the state and all its substates
	// However, we are making here a distinction for widgets (derived from UUserWidget) because those are never "destroyed"
	// but rather orphaned and collected during GC. If we passed the context object as the outer, this would create
	// a cyclic dependency and this would prevent the widget to be collected. Using the world here would still
	// satisfy 2) above while not creating adverse effects since widgets are not supposed to outlive the world
	// they have been created into. (Other kind of objects may outlive the world, for example objects used as GameInstance)
	auto rootState = NewObject<UGCFSMRootState>(context->IsA<UUserWidget>() ? context->GetWorld() : context);
	rootState->Initialize(context);
	return rootState;
}

void UGCFSMRootState::Initialize(UObject* context)
{
	UGCFSMState::Initialize(context, nullptr, false, 0.f, TEXT("<root>"));
	ticker.Emplace(this);
}

#if ENABLE_VISUAL_LOG
FString UGCFSMRootState::MakeDetailedNameForLogging() const
{
	return TEXT("root state");
}
#endif

void UGCFSMRootState::BeginDestroy()
{
	Super::BeginDestroy();
	ticker.Reset();
}

UGCFSMRootState::FTicker::FTicker(UGCFSMRootState* _state)
	: state(_state), paused(false), tickableWhenGamePaused(false)
{}

void UGCFSMRootState::FTicker::Tick(float deltaTime)
{
	if (auto contextObject = state->GetContextObject())
	{
		if (!paused)
		{
			state->PrivateTick(deltaTime);
		}
	}
	else
	{
		state->MarkAsGarbage();
		state->ticker.Reset(); // invokes destructor on *this
	}
}
