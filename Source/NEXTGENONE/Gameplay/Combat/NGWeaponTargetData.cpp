#include "Gameplay/Combat/NGWeaponTargetData.h"

bool FNGPrimaryFireView::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bOriginSuccess = false, bDirectionSuccess = false;
	Origin.NetSerialize(Ar, Map, bOriginSuccess);
	Direction.NetSerialize(Ar, Map, bDirectionSuccess);
	Ar << EquipmentRevision;
	bOutSuccess = bOriginSuccess && bDirectionSuccess && !Ar.IsError();
	return true;
}
