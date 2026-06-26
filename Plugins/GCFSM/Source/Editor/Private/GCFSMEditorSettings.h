// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMEditorSettings.generated.h"

/**
* Implements the settings for the GC FSM plugin.
*/
UCLASS(config = Editor, defaultconfig)
class GCFSMEDITOR_API UGCFSMEditorSettings : public UObject
{
	GENERATED_UCLASS_BODY()
	
public:
	/** List of transition events for newly placed state nodes. */
	UPROPERTY(config, EditAnywhere, Category = Editor)
	TArray<FName> defaultTransitionEvents;
};
