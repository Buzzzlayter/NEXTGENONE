// Exclusive publisher - Forward Gateway, LLC

#include "UIModule.h"
#if WITH_EDITOR
#include "Private/Editor/HFSMComponentCustomization.h"
#endif

void FUIModule::StartupModule()
{
#if WITH_EDITOR
	if (GIsEditor) HFSMComponentEditor::RegisterCustomization();
#endif
}

void FUIModule::ShutdownModule()
{
#if WITH_EDITOR
	if (GIsEditor) HFSMComponentEditor::UnregisterCustomization();
#endif
}

IMPLEMENT_MODULE(FUIModule, UIModule)
