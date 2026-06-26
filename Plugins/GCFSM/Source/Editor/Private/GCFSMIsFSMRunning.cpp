// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMIsFSMRunning.h"
#include "GCFSMUtilities.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"

#define LOCTEXT_NAMESPACE "GCFSM.IsFSMRunning"

void UGCFSMIsFSMRunning::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	check(Pins.Num() == ReturnPinIndex);
	auto pin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, {}, nullptr, {});
}

FSlateIcon UGCFSMIsFSMRunning::GetIconAndTint(FLinearColor& outColor) const
{
	return FSlateIcon("GCFSM", "NodeIcon.IsFSMRunning");
}

FText UGCFSMIsFSMRunning::GetNodeTitle(ENodeTitleType::Type titleType) const
{
	if (titleType != ENodeTitleType::FullTitle)
	{
		return LOCTEXT("NodeTitle", "Is FSM Running");
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Target"), GetEffectiveTargetClass()->GetDisplayNameText());
		return FText::Format(LOCTEXT("FullTitle", "Is FSM Running\nTarget is {Target}"), Args);
	}
}

FText UGCFSMIsFSMRunning::GetTooltipText() const
{
	return LOCTEXT("ToolTip", "Return true if the specified FSM is currently running");
}

void UGCFSMIsFSMRunning::ExpandNode(FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph)
{
	Super::ExpandNode(compilerContext, sourceGraph);

	auto callFunction = compilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, sourceGraph);
	callFunction->SetFromFunction(UGCFSMUtilities::StaticClass()->FindFunctionByName(TEXT("IsFSMRunning")));
	callFunction->AllocateDefaultPins();
	compilerContext.MovePinLinksToIntermediate(*Pins[InExecPinIndex], *callFunction->GetExecPin());
	compilerContext.MovePinLinksToIntermediate(*Pins[OutExecPinIndex], *callFunction->GetThenPin());
	compilerContext.MovePinLinksToIntermediate(*Pins[TargetPinIndex], *callFunction->FindPinChecked(TEXT("stateOrContext")));
	compilerContext.MovePinLinksToIntermediate(*Pins[NamePinIndex], *callFunction->FindPinChecked(TEXT("fsmName")));
	compilerContext.MovePinLinksToIntermediate(*Pins[ReturnPinIndex], *callFunction->GetReturnValuePin());
	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
