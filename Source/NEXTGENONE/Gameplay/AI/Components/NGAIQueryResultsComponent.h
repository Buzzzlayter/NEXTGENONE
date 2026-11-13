#pragma once

#include "HFSMStateComponent.h"
#include "NGAIQueryResultsComponent.generated.h"

class APawn;
class AActor;
class ANGAIController;

USTRUCT(BlueprintType)
struct FNGAIQueryItem
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="AI|EQS") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="AI|EQS") TWeakObjectPtr<AActor> Actor;
	UPROPERTY(BlueprintReadOnly, Category="AI|EQS") float Score = 0.f;
};

/** Shared by nested action states; lifetime belongs to the containing HFSM state. */
UCLASS(EditInlineNew, Blueprintable, meta=(DisplayName="AI EQS Results"))
class NEXTGENONE_API UNGAIQueryResultsComponent : public UHFSMStateComponent
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Exit(FName Event) override;
	static UNGAIQueryResultsComponent* Find(UHFSMState* FromState);
	UFUNCTION(BlueprintPure, Category="AI|EQS") bool GetQueryItem(FName ResultKey, int32 ItemIndex, FNGAIQueryItem& Item) const;
	UFUNCTION(BlueprintPure, Category="AI|EQS") int32 GetQueryItemCount(FName ResultKey) const;
	void ClearQueryResult(FName ResultKey);
	bool StoreQueryResult(FName ResultKey, APawn* ExpectedPawn, uint64 ExpectedSession, TArray<FNGAIQueryItem>&& Items);
	uint64 GetSession() const { return Session; }
private:
	bool IsCurrentLife() const;
	TWeakObjectPtr<ANGAIController> Controller;
	TWeakObjectPtr<APawn> Pawn;
	uint32 AvatarGeneration = 0;
	uint64 Session = 0;
	TMap<FName, TArray<FNGAIQueryItem>> Results;
};
