// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "LoadingProcessInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class ULoadingProcessInterface : public UInterface
{
	GENERATED_BODY()
};

class COMMONLOADINGSCREEN_API ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const = 0;
};
