#include "Gameplay/Editor/NGSetupCommandlet.h"

#if WITH_EDITOR
int32 NGMigrateEquipmentAssets();
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
	const bool bEquipment = FParse::Param(*Params, TEXT("EquipmentOnly"));
	const bool bConfigure = FParse::Param(*Params, TEXT("WeaponVFXOnly"));
	if (int32(bConfigure) + int32(bEquipment) != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("NGSetup: specify exactly one operation: -WeaponVFXOnly or -EquipmentOnly. Legacy setup/migration operations were removed."));
		return 1;
	}
	if (bEquipment) return NGMigrateEquipmentAssets();
	return NGConfigureWeaponVFX();
#else
	return 1;
#endif
}
