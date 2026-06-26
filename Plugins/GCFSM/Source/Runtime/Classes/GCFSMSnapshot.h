// Copyright (c) 2019 Gamecentric, All rights reserved

#pragma once

#include <utility>
#include "GCFSMSnapshot.generated.h"

class UGCFSMSnapshot;

USTRUCT()
struct FGCFSMSnapshotData
{
	GENERATED_BODY()

	FName fsmName;
	FGuid activeNodeGuid;

	UPROPERTY()
	TMap<FGuid, TObjectPtr<UGCFSMSnapshot>> stateHistories;

	FGCFSMSnapshotData() = default;

	FGCFSMSnapshotData(FName name, const FGuid& guid, const TMap<FGuid, TObjectPtr<UGCFSMSnapshot>>& histories)
		: fsmName(name), activeNodeGuid(guid), stateHistories(histories)
	{}
};

UCLASS(Blueprintable, ClassGroup = "GC FSM", DisplayName = "GC FSM Snapshot")
class GCFSM_API UGCFSMSnapshot : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GC FSM")
	FString ToString() const;

	UFUNCTION(BlueprintPure, Category = "GC FSM", DisplayName = "GC FSM Snapshot From String")
	static UGCFSMSnapshot* FromString(const FString& string);

	UFUNCTION(BlueprintCallable, Category = "GC FSM", DisplayName = "Are GC FSM Snapshots Equal", Meta = (Tooltip = "Compares if two snapshots contain the exact same data. Could be slow to execute. Use for debugging purposes only."))
	static bool AreSnapshotsEqual(const UGCFSMSnapshot* snapshot1, const UGCFSMSnapshot* snapshot2);

	class Builder
	{
		UGCFSMSnapshot* snapshot;

	public:
		Builder();
		Builder(UObject* outer);
		void AddFSM(FName fsmName, const FGuid& activeNodeGuid, const TMap<FGuid, TObjectPtr<UGCFSMSnapshot>>& stateHistories);
		void EndState();

		UGCFSMSnapshot* MakeFSMSnapshot()
		{
			return std::exchange(snapshot, nullptr);
		}
	};

	friend class Builder;

	using Iterator = TArray<FGCFSMSnapshotData>::TConstIterator;

	Iterator GetIterator() const
	{
		return fsmData.CreateConstIterator();
	}

private:
	UPROPERTY()
	TArray<FGCFSMSnapshotData> fsmData;

	FString ToStringImpl() const;
	static UGCFSMSnapshot* ParseSpan(const TCHAR* begin, const TCHAR* end);
};
