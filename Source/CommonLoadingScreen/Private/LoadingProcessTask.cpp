// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingProcessTask.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "LoadingScreenManager.h"

ULoadingProcessTask* ULoadingProcessTask::CreateLoadingScreenProcessTask(UObject* WorldContextObject, const FString& ShowLoadingScreenReason)
{
	UWorld* TaskWorld = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	auto* Instance = TaskWorld ? TaskWorld->GetGameInstance() : nullptr;
	auto* Manager = Instance ? Instance->GetSubsystem<ULoadingScreenManager>() : nullptr;
	if (!Manager)
	{
		return nullptr;
	}

	auto* Task = NewObject<ULoadingProcessTask>(Manager);
	Task->Reason = ShowLoadingScreenReason;
	Manager->RegisterLoadingProcessor(Task);
	return Task;
}

void ULoadingProcessTask::Unregister()
{
	if (auto* Manager = GetTypedOuter<ULoadingScreenManager>())
	{
		Manager->UnregisterLoadingProcessor(this);
	}
}

bool ULoadingProcessTask::ShouldShowLoadingScreen(FString& OutReason) const
{
	OutReason = Reason;
	return true;
}
