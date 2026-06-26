// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMPolyNodeBase.h"
#include "GCFSMEditorModule.h"
#include "EdGraphSchema_K2.h"

void UGCFSMPolyNodeBase::AllocateDefaultPins()
{
	UEdGraphPin* pin;
	check(Pins.Num() == InExecPinIndex);
	pin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, {}, nullptr, UEdGraphSchema_K2::PN_Execute);
	check(Pins.Num() == OutExecPinIndex);
	pin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, {}, nullptr, UEdGraphSchema_K2::PN_Then);
	check(Pins.Num() == TargetPinIndex);
	pin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, {}, static_cast<UObject*>(UObject::StaticClass()), UEdGraphSchema_K2::PN_Self);;
	pin->PinFriendlyName = FText::FromString(TEXT("Target"));
}

void UGCFSMPolyNodeBase::NotifyPinConnectionListChanged(UEdGraphPin* pin)
{
	Super::NotifyPinConnectionListChanged(pin);

	if (pin == Pins[TargetPinIndex])
	{
		if (const UEdGraphSchema* Schema = GetSchema())
		{
			Schema->ForceVisualizationCacheClear();
		}
	}
}

UClass* UGCFSMPolyNodeBase::GetEffectiveTargetClass() const
{
	UClass* targetClass = nullptr;
	if (Pins[TargetPinIndex]->LinkedTo.Num() > 0)
	{
		check(Pins[TargetPinIndex]->LinkedTo.Num() == 1);
		auto targetPin = Pins[TargetPinIndex]->LinkedTo[0];
		if (targetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
		{
			if (targetPin->PinType.PinSubCategory == UEdGraphSchema_K2::PSC_Self)
			{
				targetClass = GetBlueprintClassFromNode();
			}
			else
			{
				targetClass = Cast<UClass>(targetPin->PinType.PinSubCategoryObject.Get());
			}
		}
	}
	else
	{
		targetClass = GetBlueprintClassFromNode();
	}
	return targetClass;
}
