// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "Modules/ModuleManager.h"
#include "VisualLogger/VisualLogger.h"

// GCFSM Log
DECLARE_LOG_CATEGORY_EXTERN(LogGCFSM, Log, All);

// My LOG/VLOG macros
#define GC_LOG(Verbosity, Format, ...)				UE_LOG(LogGCFSM, Verbosity, Format, ##__VA_ARGS__)
#define GC_VLOG(Context, Verbosity, Format,...)		UE_VLOG(Context, LogGCFSM, Verbosity, Format, ##__VA_ARGS__)
#define GC_VLOGL(Context, Verbosity, Format,...)	UE_VLOG_UELOG(Context, LogGCFSM, Verbosity, Format, ##__VA_ARGS__)

/**
* The public interface to this module.  In most cases, this interface is only public to sibling modules
* within this plugin.
*/
class IGCFSMModule : public IModuleInterface
{

public:

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IGCFSMModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IGCFSMModule >("GCFSMModule");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("GCFSMModule");
	}
};

