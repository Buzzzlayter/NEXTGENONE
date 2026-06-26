// Copyright (c) 2019 Gamecentric, All rights reserved

#include "GCFSMSnapshot.h"
#include "GCFSMModule.h"
#include <algorithm>

static const TCHAR snapshotPrefix[] = TEXT("GCM10");
static constexpr int snapshotPrefixLen = sizeof(snapshotPrefix) / sizeof(snapshotPrefix[0]) - 1;

FString UGCFSMSnapshot::ToString() const
{
	return snapshotPrefix + ToStringImpl();
}

FString UGCFSMSnapshot::ToStringImpl() const
{
	FString result;
	for (const auto& fsm : fsmData)
	{
		if (fsm.fsmName.IsNone())
		{
			result += TEXT('$');
		}
		else
		{
			result += fsm.fsmName.ToString();
			result += TEXT(';');
			result += fsm.activeNodeGuid.ToString();
			result += TEXT('.');
			for (auto& history : fsm.stateHistories)
			{
				result += TEXT('<');
				result += history.Key.ToString();
				result += TEXT(';');
				result += history.Value->ToStringImpl();
				result += TEXT('>');
			}
		}
	}
	return result;
}

namespace
{
	TOptional<FName> ParseName(const TCHAR*& begin, const TCHAR* end, TCHAR delim)
	{
		auto pdelim = std::find(begin, end, delim);
		if (pdelim != end)
		{
			FName name{ *FString{ static_cast<int>(pdelim - begin), begin } };
			if (name.IsValid())
			{
				begin = pdelim + 1;
				return name;
			}
		}
		return {};
	}

	TOptional<FGuid> ParseGuid(const TCHAR*& begin, const TCHAR* end, TCHAR delim)
	{
		auto pdelim = std::find(begin, end, delim);
		if (pdelim != end)
		{
			FGuid guid;
			if (FGuid::Parse(FString{ static_cast<int>(pdelim - begin), begin }, guid))
			{
				begin = pdelim + 1;
				return guid;
			}
		}
		return {};
	}
}

UGCFSMSnapshot* UGCFSMSnapshot::FromString(const FString& string)
{
	if (!string.StartsWith(snapshotPrefix))
	{
		GC_LOG(Error, TEXT("Invalid snapshot string '%s'"), *string);
		return nullptr;
	}

	auto ptr = &string[0];
	auto end = ptr + string.Len();
	auto result = ParseSpan(ptr + snapshotPrefixLen, end);
	if (result == nullptr)
	{
		GC_LOG(Error, TEXT("Invalid snapshot string '%s'"), *string);
	}
	return result;
}

UGCFSMSnapshot* UGCFSMSnapshot::ParseSpan(const TCHAR* begin, const TCHAR* end)
{
	Builder builder;
	FGuid guid;

	while (begin != end)
	{
		if (*begin == '$')
		{
			builder.EndState();
			++begin;
		}
		else
		{
			auto name{ ParseName(begin, end, ';') };
			if (!name.IsSet())
			{
				return nullptr;
			}

			auto guid{ ParseGuid(begin, end, '.') };
			if (!guid.IsSet())
			{
				return nullptr;
			}

			TMap<FGuid, TObjectPtr<UGCFSMSnapshot>> history;

			while (begin != end && *begin == '<')
			{
				++begin;
				auto nodeguid{ ParseGuid(begin, end, ';') };
				if (!nodeguid.IsSet())
				{
					return nullptr;
				}

				auto pdelim = std::find(begin, end, '>');
				if (pdelim == end)
				{
					return nullptr;
				}

				history.Add(nodeguid.GetValue(), ParseSpan(begin, pdelim));
				begin = pdelim + 1;
			}

			builder.AddFSM(name.GetValue(), guid.GetValue(), history);
		}
	}

	return builder.MakeFSMSnapshot();
}

namespace
{
	bool CompareMaps(const TMap<FGuid, TObjectPtr<UGCFSMSnapshot>>& map1, const TMap<FGuid, TObjectPtr<UGCFSMSnapshot>>& map2)
	{
		if (map1.Num() != map2.Num())
		{
			return false;
		}

		for (auto& elem1 : map1)
		{
			auto value2 = map2.Find(elem1.Key);
			if (value2 == nullptr || !UGCFSMSnapshot::AreSnapshotsEqual(elem1.Value, *value2))
			{
				return false;
			}
		}

		return true;
	}
}

bool UGCFSMSnapshot::AreSnapshotsEqual(const UGCFSMSnapshot* snapshot1, const UGCFSMSnapshot* snapshot2)
{
	if (snapshot1 == snapshot2)
	{
		return true;
	}
	else if (snapshot1 == nullptr || snapshot2 == nullptr)
	{
		return false;
	}

	auto sameData = [](const FGCFSMSnapshotData& data1, const FGCFSMSnapshotData& data2) -> bool
	{
		return data1.fsmName == data2.fsmName && data1.activeNodeGuid == data2.activeNodeGuid && CompareMaps(data1.stateHistories, data2.stateHistories);
	};

	// TArray do not play nicely with std algorithms
	auto begin = [](auto& array) { return array.GetData(); };
	auto end = [](auto& array) { return array.GetData() + array.Num(); };
	return std::equal(begin(snapshot1->fsmData), end(snapshot1->fsmData), begin(snapshot2->fsmData), end(snapshot2->fsmData), sameData);
}

UGCFSMSnapshot::Builder::Builder()
{
	snapshot = NewObject<UGCFSMSnapshot>();
}

UGCFSMSnapshot::Builder::Builder(UObject* outer)
{
	snapshot = NewObject<UGCFSMSnapshot>(outer);
}

void UGCFSMSnapshot::Builder::AddFSM(FName fsmName, const FGuid& activeNodeGuid, const TMap<FGuid, TObjectPtr<UGCFSMSnapshot>>& stateHistories)
{
	snapshot->fsmData.Emplace(fsmName, activeNodeGuid, stateHistories);
}

void UGCFSMSnapshot::Builder::EndState()
{
	snapshot->fsmData.AddDefaulted();
}
