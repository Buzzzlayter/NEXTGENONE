// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMEditorSettings.h"

UGCFSMEditorSettings::UGCFSMEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	defaultTransitionEvents.Add(TEXT("Event"));
}
