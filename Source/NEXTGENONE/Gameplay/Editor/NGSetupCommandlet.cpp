#include "Gameplay/Editor/NGSetupCommandlet.h"

#if WITH_EDITOR
int32 NGImportWeaponVFX();
int32 NGConfigureWeaponVFX();
#endif

UNGSetupCommandlet::UNGSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UNGSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bImport = FParse::Param(*Params, TEXT("ImportWeaponVFX"));
	const bool bConfigure = FParse::Param(*Params, TEXT("WeaponVFXOnly"));
	if (bImport == bConfigure)
	{
		UE_LOG(LogTemp, Error, TEXT("NGSetup: specify exactly one operation: -ImportWeaponVFX or -WeaponVFXOnly. Legacy setup/migration operations were removed."));
		return 1;
	}
	return bImport ? NGImportWeaponVFX() : NGConfigureWeaponVFX();
#else
	return 1;
#endif
}
