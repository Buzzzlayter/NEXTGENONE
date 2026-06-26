// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "Factories/BlueprintFactory.h"
#include "GCFSMState.h"
#include "GCFSMStateFactory.generated.h"

UCLASS()
class UGCFSMStateFactory : public UBlueprintFactory
{
public:
	GENERATED_BODY()

public:
	bool ShouldShowInNewMenu() const override
	{
		return false;
	}

	bool ConfigureProperties() override
	{
		ParentClass = UGCFSMState::StaticClass();
		return true;
	}
};
