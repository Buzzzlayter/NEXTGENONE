#include "Gameplay/AI/States/NGAIQueryState.h"

#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/Components/NGAIQueryResultsComponent.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "GameFramework/Pawn.h"

void UNGAIQueryState::StartAction()
{
	auto* Results = UNGAIQueryResultsComponent::Find(this);
	if (!Results)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Missing EQS results component in state hierarchy")); return; }
	if (!ResultKey.IsNone()) Results->ClearQueryResult(ResultKey);
	if (!QueryTemplate || ResultKey.IsNone() || RunMode.GetValue() > EEnvQueryRunMode::AllMatching)
	{ FinishAction(ENGAIActionResult::Failed, TEXT("Invalid EQS template, result key or run mode")); return; }
	FEnvQueryRequest Request(QueryTemplate.Get(), ExpectedPawn.Get());
	for (const auto& Param : QueryParams)
	{
		if (Param.ParamName.IsNone() || !FMath::IsFinite(Param.Value))
		{ FinishAction(ENGAIActionResult::Failed, TEXT("Invalid EQS parameter")); return; }
		switch (Param.ParamType)
		{
		case EAIParamType::Float: Request.SetFloatParam(Param.ParamName, Param.Value); break;
		case EAIParamType::Bool: Request.SetBoolParam(Param.ParamName, Param.Value != 0.f); break;
		case EAIParamType::Int:
			if (static_cast<double>(Param.Value) < MIN_int32 || static_cast<double>(Param.Value) > MAX_int32)
			{ FinishAction(ENGAIActionResult::Failed, TEXT("EQS integer parameter out of range")); return; }
			Request.SetIntParam(Param.ParamName, static_cast<int32>(Param.Value)); break;
		default: FinishAction(ENGAIActionResult::Failed, TEXT("Unsupported EQS parameter type")); return;
		}
	}
	const uint64 StartedRun = RunSerial;
	const FName OutputKey = ResultKey;
	const TWeakObjectPtr<UNGAIQueryResultsComponent> Output = Results;
	const uint64 OutputSession = Results->GetSession();
	QueryID = Request.Execute(RunMode, FQueryFinishedSignature::CreateWeakLambda(this,
		[this, StartedRun, OutputKey, Output, OutputSession](TSharedPtr<FEnvQueryResult> QueryResult)
	{
		if (!IsCurrentRun(StartedRun)) return;
		QueryID = INDEX_NONE;
		if (!ActionController->GetReadyAbilitySystem(ExpectedPawn.Get()))
		{ FinishAction(ENGAIActionResult::Aborted, TEXT("Gameplay readiness lost during EQS")); return; }
		if (!QueryResult.IsValid() || !QueryResult->IsSuccessful() || QueryResult->Items.IsEmpty()
			|| !QueryResult->ItemType || !QueryResult->ItemType->IsChildOf(UEnvQueryItemType_VectorBase::StaticClass()))
		{ FinishAction(ENGAIActionResult::Failed, TEXT("EQS failed, empty or unsupported result")); return; }
		const bool bActorItems = QueryResult->ItemType->IsChildOf(UEnvQueryItemType_ActorBase::StaticClass());
		TArray<FNGAIQueryItem> Items;
		Items.Reserve(QueryResult->Items.Num());
		for (int32 Index = 0; Index < QueryResult->Items.Num(); ++Index)
		{
			FNGAIQueryItem Item;
			Item.Actor = bActorItems ? QueryResult->GetItemAsActor(Index) : nullptr;
			Item.Location = QueryResult->GetItemAsLocation(Index);
			Item.Score = QueryResult->GetItemScore(Index);
			if ((bActorItems && !Item.Actor.IsValid()) || Item.Location.ContainsNaN() || !FMath::IsFinite(Item.Score))
			{ FinishAction(ENGAIActionResult::Failed, TEXT("Invalid EQS result item")); return; }
			Items.Add(Item);
		}
		if (!Output.IsValid() || !Output->StoreQueryResult(OutputKey, ExpectedPawn.Get(), OutputSession, MoveTemp(Items)))
		{ FinishAction(ENGAIActionResult::Failed, TEXT("EQS result owner no longer active")); return; }
		FinishAction(ENGAIActionResult::Succeeded, TEXT("EQS result stored"));
	}));
	if (QueryID == INDEX_NONE) FinishAction(ENGAIActionResult::Failed, TEXT("EQS request rejected"));
}

void UNGAIQueryState::ReleaseActionResources()
{
	const int32 PendingQuery = QueryID;
	QueryID = INDEX_NONE;
	if (PendingQuery != INDEX_NONE)
		if (auto* Manager = UEnvQueryManager::GetCurrent(GetWorld())) Manager->AbortQuery(PendingQuery);
}
