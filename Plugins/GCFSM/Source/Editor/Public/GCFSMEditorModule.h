// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "Math/Color.h"

// General Log
DECLARE_LOG_CATEGORY_EXTERN(LogGCFSM, Log, All);

// Base color for all GCFSM nodes
const FLinearColor GCFSMNodeColor { 0, 0.607843f, 0.568627f };

// Default menu category for all GCFSM nodes
class FText GCFSMMenuCategory();

/**
* The public interface to this module.  In most cases, this interface is only public to sibling modules
* within this plugin.
*/
class IGCFSMEditorModule : public IModuleInterface
{

public:

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IGCFSMEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IGCFSMEditorModule >("GCFSMEditorModule");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("GCFSMEditorModule");
	}
};

