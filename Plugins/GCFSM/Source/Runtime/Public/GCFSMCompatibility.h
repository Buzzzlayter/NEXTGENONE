// Copyright (c) 2020 Gamecentric, All rights reserved

#pragma once

#if __has_include("Runtime/Launch/Resources/Version.h")
#include "Runtime/Launch/Resources/Version.h"
#else
#include "../Launch/Resources/Version.h"
#endif

// in Unreal Engine 4.25 UProperty and all related classes have been renamed to FProperty etc.

#if ENGINE_MAJOR_VERSION ==	4 && ENGINE_MINOR_VERSION <= 24

using FProperty = UProperty;
using FNumericProperty = UNumericProperty;
using FObjectProperty = UObjectProperty;
using FMulticastDelegateProperty = UMulticastDelegateProperty;

template <class T, class Param>
inline T* FindUField(const UStruct* Owner, Param&& FieldName)
{
	return FindField<T>(Owner, std::forward<Param>(FieldName));
}

template <class T, class Param>
inline T* FindFProperty(const UStruct* Owner, Param&& FieldName)
{
	return FindField<T>(Owner, std::forward<Param>(FieldName));
}

template<typename FieldType>
inline FieldType* CastField(UField* Src)
{
	return Cast<FieldType>(Src);
}

template<typename FieldType>
inline const FieldType* CastField(const UField* Src)
{
	return Cast<FieldType>(Src);
}

#else

#endif