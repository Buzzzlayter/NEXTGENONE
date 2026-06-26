// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "UObject/Class.h"

namespace EnumHelpers
{
	template <class Enum> struct EnumName;

#define ENUM_HELPERS_NAME(Enum, Module) \
	template <> \
	struct ::EnumHelpers::EnumName<Enum> \
	{ \
		static constexpr const TCHAR* name = TEXT(#Enum); \
		static constexpr const TCHAR* module = TEXT("/Script/") TEXT(#Module); \
	};

	template <class Enum>
	inline UEnum* GetUEnum()
	{
		static UEnum* result = []
		{
			FTopLevelAssetPath EnumPath(EnumName<Enum>::module, EnumName<Enum>::name);
			return FindObject<UEnum>(EnumPath);
		}();
		return result;
	}

	template <class Enum>
	inline int32 GetIndex(Enum value)
	{
		return GetUEnum<Enum>()->GetEnumIndex(static_cast<int64>(value));
	}

	template <class Enum>
	inline Enum GetValue(int32 index)
	{
		return static_cast<Enum>(GetUEnum<Enum>()->GetValueByIndex(index));
	}

	template <class Enum>
	inline FName GetName(int32 index)
	{
		return GetUEnum<Enum>()->GetNameByIndex(index);
	}

	template <class Enum>
	inline int32 GetIndex(FName name, EGetByNameFlags flags = EGetByNameFlags::None)
	{
		return GetUEnum<Enum>()->GetIndexByName(name, flags);
	}

	template <class Enum>
	inline FName GetName(Enum value)
	{
		return GetUEnum<Enum>()->GetNameByValue(static_cast<int64>(value));
	}

	template <class Enum>
	inline Enum GetValue(FName name, EGetByNameFlags flags = EGetByNameFlags::None)
	{
		return static_cast<Enum>(GetUEnum<Enum>()->GetValueByName(name, flags));
	}

	template <class Enum>
	inline FString GetNameString(int32 index)
	{
		return GetUEnum<Enum>()->GetNameStringByIndex(index);
	}

	template <class Enum>
	inline int32 GetIndex(const FString& searchString, EGetByNameFlags flags = EGetByNameFlags::None)
	{
		return GetUEnum<Enum>()->GetIndexByNameString(searchString, flags);
	}

	template <class Enum>
	inline FString GetNameString(Enum value)
	{
		return GetUEnum<Enum>()->GetNameStringByValue(static_cast<int64>(value));
	}

	template <class Enum>
	inline Enum GetValue(const FString& searchString, EGetByNameFlags flags = EGetByNameFlags::None)
	{
		return static_cast<Enum>(GetUEnum<Enum>()->GetValueByNameString(searchString, flags));
	}

	template <class Enum>
	inline FText GetDisplayNameText(int32 index)
	{
		return GetUEnum<Enum>()->GetDisplayNameTextByIndex(index);
	}

	template <class Enum>
	inline FText GetDisplayNameText(int64 value)
	{
		return GetUEnum<Enum>()->GetDisplayNameTextByValue(static_cast<int64>(value));
	}
}