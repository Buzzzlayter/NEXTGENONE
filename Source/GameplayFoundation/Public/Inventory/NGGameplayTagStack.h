#pragma once

#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "NGGameplayTagStack.generated.h"

struct FNGGameplayTagStackContainer;
struct FNetDeltaSerializeInfo;


USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGGameplayTagStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FNGGameplayTagStack()
	{}

	FNGGameplayTagStack(FGameplayTag InTag, int32 InStackCount)
		: Tag(InTag)
		, StackCount(InStackCount)
	{
	}

	FString GetDebugString() const;

private:
	friend FNGGameplayTagStackContainer;

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 StackCount = 0;
};

/** Container of gameplay tag stacks */
USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGGameplayTagStackContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	FNGGameplayTagStackContainer()
	//	: Owner(nullptr)
	{
	}

public:
	TFunction<void()> OnReplicatedChange;
	TMap<FGameplayTag, int32> GetAllCounts() const { return TagToCountMap; }
	// Adds a specified number of stacks to the tag (does nothing if StackCount is below 1)
	void AddStack(FGameplayTag Tag, int32 StackCount);

	// Removes up to StackCount; missing tags and nonpositive counts are no-ops. Exhausted tags are removed.
	void RemoveStack(FGameplayTag Tag, int32 StackCount);

	// Returns the stack count of the specified tag (or 0 if the tag is not present)
	int32 GetStackCount(FGameplayTag Tag) const
	{
		return TagToCountMap.FindRef(Tag);
	}

	// Returns true if there is at least one stack of the specified tag
	bool ContainsTag(FGameplayTag Tag) const
	{
		return TagToCountMap.Contains(Tag);
	}

	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FNGGameplayTagStack, FNGGameplayTagStackContainer>(Stacks, DeltaParms, *this);
	}

private:
	// Replicated list of gameplay tag stacks
	UPROPERTY()
	TArray<FNGGameplayTagStack> Stacks;
	
	// Accelerated list of tag stacks for queries
	TMap<FGameplayTag, int32> TagToCountMap;
};

template<>
struct TStructOpsTypeTraits<FNGGameplayTagStackContainer> : public TStructOpsTypeTraitsBase2<FNGGameplayTagStackContainer>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
