// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMModule.h"
#include <vector>
#include <memory>

class FGCFSMModule : public IGCFSMModule
{
	/** IModuleInterface implementation */
	void StartupModule() override;
	void ShutdownModule() override;

private:
};

// General Log
DEFINE_LOG_CATEGORY(LogGCFSM);

IMPLEMENT_MODULE(FGCFSMModule, GCFSM)

void FGCFSMModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}

void FGCFSMModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}
