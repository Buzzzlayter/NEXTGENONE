// Copyright (c) 2024 Gamecentric, All rights reserved

#include "GCFSMSubsystem.h"

#include "GCFSMModule.h"
#include "GCFSMRootState.h"

UGCFSMSubsystem* UGCFSMSubsystem::instance;

void UGCFSMSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);

	if (instance)
	{
		GC_LOG(Error, TEXT("GCFSMSubsystem::Initialize already called"));
	}
	else
	{
		instance = this;
		FWorldDelegates::OnWorldCleanup.AddUObject(this, &UGCFSMSubsystem::OnWorldCleanup);
		FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddUObject(this, &UGCFSMSubsystem::OnPreGarbageCollect);
	}
}

void UGCFSMSubsystem::Deinitialize()
{
	Super::Deinitialize();

	if (instance == this)
	{
		instance = nullptr;
	}
}

void UGCFSMSubsystem::OnPreGarbageCollect()
{
	for (auto it = activeContexts.CreateIterator(); it; ++it)
	{
		if (auto rootState = it->Value.Get(); rootState == nullptr || rootState->GetContextObject() == nullptr)
		{
			it.RemoveCurrent();
		}
	}
}
void UGCFSMSubsystem::OnWorldCleanup(UWorld* world, bool, bool)
{
	for (auto it = activeContexts.CreateIterator(); it; ++it)
	{
		if (auto rootState = it->Value.Get(); rootState == nullptr || rootState->GetContextObject() == nullptr || rootState->GetWorld() == world)
		{
			it.RemoveCurrent();
		}
	}
}

UGCFSMRootState * UGCFSMSubsystem::GetOrMakeRootStateFromContext(UObject* context)
{
	check(instance);
	if (IsValid(context))
	{
		auto contextId = reinterpret_cast<uint64>(context);
		auto& rootState = instance->activeContexts.FindOrAdd(contextId);
		if (!rootState)
		{
			rootState = UGCFSMRootState::MakeRootStateObject(context);
		}
		return rootState;
	}
	else
	{
		GC_LOG(Error, TEXT("Object %s is invalid, can't run FSMs"), (context ? *context->GetName() : TEXT("<null>")));
	}
	return nullptr;
}

UGCFSMRootState* UGCFSMSubsystem::GetRootStateFromContext(UObject* context, bool verbose)
{
	if (IsValid(context))
	{
		auto contextId = reinterpret_cast<uint64>(context);
		if (auto* rootState = instance->activeContexts.Find(contextId))
		{
			return rootState->Get();
		}
		else if (verbose)
		{
			GC_LOG(Error, TEXT("Object %s is not running FSMs"), *context->GetName());
		}
	}
	else
	{
		GC_LOG(Error, TEXT("Object %s is invalid, can't run FSMs"), (context ? *context->GetName() : TEXT("<null>")));
	}
	return nullptr;
}
