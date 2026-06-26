// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMGetActiveStateNode.h"
#include "GCFSMEditorModule.h"
#include "GCFSMUtilities.h"
#include "GCFSMState.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"

#define LOCTEXT_NAMESPACE "GCFSM.GetActiveStateNode"

void UGCFSMGetActiveStateNode::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	auto schema = GetDefault<UEdGraphSchema_K2>();

	check(Pins.Num() == StateClassPinIndex);
	auto pin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Class, {}, UGCFSMBasicState::StaticClass(), "State Class");
	pin->PinToolTip = LOCTEXT("StateClassPin.Tooltip", "The returned state is automatically cast to this class").ToString();
	pin->DefaultObject = UGCFSMBasicState::StaticClass();

	check(Pins.Num() == OutputPinIndex);
	pin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object, {}, nullptr, "State");
	pin->PinToolTip = LOCTEXT("OutputPin.Tooltip", "The active state of the specified FSM").ToString();

	check(Pins.Num() == NoStatePinIndex);
	pin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, {}, nullptr, "No State");
	pin->PinToolTip = LOCTEXT("NoStatePin.Tooltip", "This pin is executed if the FSM has no active state or if the cast fails").ToString();
}

void UGCFSMGetActiveStateNode::UpdateOutputPinType()
{
	auto stateClassPin = Pins[StateClassPinIndex];
	UObject* stateClass = UGCFSMBasicState::StaticClass();
	if (stateClassPin->LinkedTo.Num() == 0 && stateClassPin->DefaultObject)
	{
		stateClass = stateClassPin->DefaultObject;
	}

	auto outputPin = Pins[OutputPinIndex];
	if (outputPin->PinType.PinSubCategoryObject.Get() != stateClass)
	{
		outputPin->Modify();
		outputPin->PinType.PinSubCategoryObject = stateClass;
		PinTypeChanged(outputPin);
	}
}

UGCFSMGetActiveStateNode::ERedirectType UGCFSMGetActiveStateNode::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	// treat the state class pin specially, because changing its type might make it mismatch the old pin
	if (NewPinIndex == OutputPinIndex && OldPin->GetName() == TEXT("State"))
	{
		return ERedirectType::ERedirectType_Name;
	}
	else
	{
		return Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
	}
}

void UGCFSMGetActiveStateNode::PostReconstructNode()
{
	UpdateOutputPinType();
	Super::PostReconstructNode();
}

void UGCFSMGetActiveStateNode::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();
	UpdateOutputPinType();
}

void UGCFSMGetActiveStateNode::PinDefaultValueChanged(UEdGraphPin* pin)
{
	Super::PinDefaultValueChanged(pin);
	if (pin == Pins[StateClassPinIndex])
	{
		UpdateOutputPinType();
	}
}

FSlateIcon UGCFSMGetActiveStateNode::GetIconAndTint(FLinearColor& outColor) const
{
	return FSlateIcon("GCFSM", "NodeIcon.GetActiveState");
}

FText UGCFSMGetActiveStateNode::GetNodeTitle(ENodeTitleType::Type titleType) const
{
	if (titleType != ENodeTitleType::FullTitle)
	{
		return LOCTEXT("NodeTitle", "Get FSM Active State");
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Target"), GetEffectiveTargetClass()->GetDisplayNameText());
		return FText::Format(LOCTEXT("FullTitle", "Get FSM Active State\nTarget is {Target}"), Args);
	}
}

FText UGCFSMGetActiveStateNode::GetTooltipText() const
{
	return LOCTEXT("ToolTip", "Obtains a reference to the currently active state of the specified FSM, if any");
}

void UGCFSMGetActiveStateNode::ExpandNode(FKismetCompilerContext& compilerContext, UEdGraph* sourceGraph)
{
	Super::ExpandNode(compilerContext, sourceGraph);

	auto callFunction = compilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, sourceGraph);
	callFunction->SetFromFunction(UGCFSMUtilities::StaticClass()->FindFunctionByName(TEXT("GetActiveState")));
	callFunction->AllocateDefaultPins();
	compilerContext.MovePinLinksToIntermediate(*Pins[InExecPinIndex], *callFunction->GetExecPin());
	compilerContext.MovePinLinksToIntermediate(*Pins[TargetPinIndex], *callFunction->FindPinChecked(TEXT("stateOrContext")));
	compilerContext.MovePinLinksToIntermediate(*Pins[NamePinIndex], *callFunction->FindPinChecked(TEXT("fsmName")));
	auto classPin = callFunction->FindPinChecked(TEXT("stateClass"));
	compilerContext.MovePinLinksToIntermediate(*Pins[StateClassPinIndex], *classPin);
	callFunction->PinConnectionListChanged(classPin); // forces callFunction node to compute the correct type of the return value pin, that uses "DeterminesOutputType"
	compilerContext.MovePinLinksToIntermediate(*Pins[OutputPinIndex], *callFunction->GetReturnValuePin());
	compilerContext.MovePinLinksToIntermediate(*Pins[OutExecPinIndex], *callFunction->FindPinChecked(TEXT("State")));
	compilerContext.MovePinLinksToIntermediate(*Pins[NoStatePinIndex], *callFunction->FindPinChecked(TEXT("NoState")));
	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
