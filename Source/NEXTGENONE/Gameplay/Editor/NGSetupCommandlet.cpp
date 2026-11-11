#include "Gameplay/Editor/NGSetupCommandlet.h"

#if WITH_EDITOR
int32 NGMigrateEquipmentAssets();
int32 NGMigrateWeaponSlotAssets();
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
	const bool bWeaponSlot = FParse::Param(*Params, TEXT("WeaponSlotOnly"));
	const bool bConfigure = FParse::Param(*Params, TEXT("WeaponVFXOnly"));
	if (int32(bConfigure) + int32(bEquipment) + int32(bWeaponSlot) != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("NGSetup: specify exactly one operation: -WeaponVFXOnly, -EquipmentOnly or -WeaponSlotOnly. Legacy setup/migration operations were removed."));
		return 1;
	}
	if (bEquipment) return NGMigrateEquipmentAssets();
	if (bWeaponSlot) return NGMigrateWeaponSlotAssets();
	return NGConfigureWeaponVFX();
#else
	return 1;
#endif
}
