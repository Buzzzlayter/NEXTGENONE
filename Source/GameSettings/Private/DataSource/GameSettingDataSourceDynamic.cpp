// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataSource/GameSettingDataSourceDynamic.h"

#include "Engine/LocalPlayer.h"

//--------------------------------------
// FGameSettingDataSourceDynamic
//--------------------------------------

FGameSettingDataSourceDynamic::FGameSettingDataSourceDynamic(const TArray<FString>& InDynamicPath)
	: DynamicPath(InDynamicPath)
{
}

FGameSettingDataSourceDynamic::FGameSettingDataSourceDynamic(UObject* InRoot, const FString& InMember)
	: ExplicitRoot(InRoot), bUseExplicitRoot(true), DynamicPath(InMember)
{
}

UObject* FGameSettingDataSourceDynamic::GetRoot(ULocalPlayer* InLocalPlayer) const
{
	return bUseExplicitRoot ? ExplicitRoot.Get() : InLocalPlayer;
}

bool FGameSettingDataSourceDynamic::Resolve(ULocalPlayer* InLocalPlayer)
{
	UObject* Root = GetRoot(InLocalPlayer);
	return Root && DynamicPath.Resolve(Root);
}

FString FGameSettingDataSourceDynamic::GetValueAsString(ULocalPlayer* InLocalPlayer) const
{
	FString OutStringValue;

	const bool bSuccess = PropertyPathHelpers::GetPropertyValueAsString(GetRoot(InLocalPlayer), DynamicPath, OutStringValue);
	ensure(bSuccess);

	return OutStringValue;
}

void FGameSettingDataSourceDynamic::SetValue(ULocalPlayer* InLocalPlayer, const FString& InStringValue)
{
	const bool bSuccess = PropertyPathHelpers::SetPropertyValueFromString(GetRoot(InLocalPlayer), DynamicPath, InStringValue);
	ensure(bSuccess);
}

FString FGameSettingDataSourceDynamic::ToString() const
{
	return DynamicPath.ToString();
}
