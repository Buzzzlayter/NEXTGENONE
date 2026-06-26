// Exclusive publisher - Forward Gateway, LLC

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define GENCHECK(Condition, ErrorMessage) \
	do \
	{ \
		if (!(Condition)) \
		{ \
			ensureAlwaysMsgf(false, TEXT("%s"), TEXT(ErrorMessage)); \
			return; \
		} \
	} while (false)

class FUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
