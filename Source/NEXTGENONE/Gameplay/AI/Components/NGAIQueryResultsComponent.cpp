#include "Gameplay/AI/Components/NGAIQueryResultsComponent.h"

#include "HFSMState.h"
#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "GameFramework/Pawn.h"

void UNGAIQueryResultsComponent::Enter()
{
	++Session;
	Results.Reset();
	Controller = Cast<ANGAIController>(GetContextOwner());
	auto* AI = Controller.Get();
	Pawn = AI ? AI->GetPawn() : nullptr;
	const auto* ASC = AI ? Cast<UNGPlayerAbilitySystemComponent>(AI->GetReadyAbilitySystem(Pawn.Get())) : nullptr;
	AvatarGeneration = ASC ? ASC->GetAvatarGeneration() : 0;
	if (!ASC) Pawn.Reset();
	Super::Enter();
}

void UNGAIQueryResultsComponent::Exit(FName Event)
{
	++Session;
	Results.Reset();
	Pawn.Reset();
	Controller.Reset();
	Super::Exit(Event);
}

bool UNGAIQueryResultsComponent::IsCurrentLife() const
{
	auto* AI = Controller.Get();
	if (!Pawn.IsValid() || !AI || AI->GetPawn() != Pawn.Get()) return false;
	const auto* ASC = Cast<UNGPlayerAbilitySystemComponent>(AI->GetReadyAbilitySystem(Pawn.Get()));
	return ASC && ASC->GetAvatarGeneration() == AvatarGeneration;
}

UNGAIQueryResultsComponent* UNGAIQueryResultsComponent::Find(UHFSMState* FromState)
{
	UHFSMStateComponent* Component = nullptr;
	if (FromState) FromState->GetClosestComponentInHierarchyUp(StaticClass(), Component);
	return Cast<UNGAIQueryResultsComponent>(Component);
}

bool UNGAIQueryResultsComponent::GetQueryItem(FName ResultKey, int32 ItemIndex, FNGAIQueryItem& Item) const
{
	Item = FNGAIQueryItem();
	const auto* Items = IsCurrentLife() ? Results.Find(ResultKey) : nullptr;
	if (!Items || !Items->IsValidIndex(ItemIndex)) return false;
	Item = (*Items)[ItemIndex];
	return true;
}

int32 UNGAIQueryResultsComponent::GetQueryItemCount(FName ResultKey) const
{
	const auto* Items = IsCurrentLife() ? Results.Find(ResultKey) : nullptr;
	return Items ? Items->Num() : 0;
}

void UNGAIQueryResultsComponent::ClearQueryResult(FName ResultKey)
{
	Results.Remove(ResultKey);
}

bool UNGAIQueryResultsComponent::StoreQueryResult(FName ResultKey, APawn* ExpectedPawn, uint64 ExpectedSession, TArray<FNGAIQueryItem>&& Items)
{
	if (ResultKey.IsNone() || ExpectedSession != Session || ExpectedPawn != Pawn.Get() || !IsCurrentLife()) return false;
	Results.Add(ResultKey, MoveTemp(Items));
	return true;
}
