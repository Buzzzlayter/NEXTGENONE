#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/NetSerialization.h"
#include "NGWeaponTargetData.generated.h"

USTRUCT()
struct NEXTGENONE_API FNGPrimaryFireView : public FGameplayAbilityTargetData
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize Origin = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantizeNormal Direction = FVector::ForwardVector;
	UPROPERTY() int32 EquipmentRevision = INDEX_NONE;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FNGPrimaryFireView> : TStructOpsTypeTraitsBase2<FNGPrimaryFireView>
{
	enum { WithNetSerializer = true, WithCopy = true };
};
