// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LoadingProcessInterface.h"
#include "LoadingProcessTask.generated.h"

UCLASS(BlueprintType)
class COMMONLOADINGSCREEN_API ULoadingProcessTask : public UObject, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Loading Screen", meta=(WorldContext="WorldContextObject"))
	static ULoadingProcessTask* CreateLoadingScreenProcessTask(UObject* WorldContextObject, const FString& ShowLoadingScreenReason);

	UFUNCTION(BlueprintCallable, Category="Loading Screen")
	void Unregister();

	bool ShouldShowLoadingScreen(FString& OutReason) const override;

private:
	FString Reason;
};
