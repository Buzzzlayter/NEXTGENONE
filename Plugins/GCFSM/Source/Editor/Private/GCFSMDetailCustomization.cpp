// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMDetailCustomization.h"
#include "GCFSMEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "ObjectEditorUtils.h"
#include "K2Node.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GCFSM.DetailCustomization"

class FGCFSMDetailCustomization : public IDetailCustomization
{
public:
	void CustomizeDetails(class IDetailLayoutBuilder& detailBuilder) override
	{
		auto& selectedObjectsList = detailBuilder.GetSelectedObjects();

		if (selectedObjectsList.Num() != 1)
		{
			// don't do any customization unless only one object is selected
		}
		else if (auto node = Cast<IGCFSMDetailCustomization>(selectedObjectsList[0].Get()))
		{
			node->Initialize();

			TSharedPtr<SWidget> nameWidget;
			TSharedPtr<SWidget> valueWidget;
			FDetailWidgetRow row;

			const FText& asPinText = LOCTEXT("AsPin", "(As pin)");
			const FText& asPinTooltip = LOCTEXT("AsPinTooltip", "Show this property as a pin on the node");

			for (auto const& pinProperty : node->pinProperties)
			{
				auto property = detailBuilder.GetProperty(pinProperty.property->GetFName());

				if (!node->IsPinPropertyAvailable(pinProperty))
				{
					property->MarkHiddenByCustomization();
					continue;
				}

				auto& category = detailBuilder.EditCategory(*FObjectEditorUtils::GetCategory(pinProperty.property));
				auto& propertyRow = category.AddProperty(property);

				if (pinProperty.pinName.IsEmpty())
				{
					continue;
				}

				propertyRow.ToolTip(FText::FromString(pinProperty.toolTip));
				propertyRow.GetDefaultWidgets(nameWidget, valueWidget, row);

				using FIsChecked = TAttribute<ECheckBoxState>;

				nameWidget = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.OnCheckStateChanged_Lambda([node, &pinProperty](ECheckBoxState state) { node->SetPinVisible(pinProperty, state == ECheckBoxState::Checked); })
						.IsChecked_Lambda([node, &pinProperty]() { return node->IsPinVisible(pinProperty) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.ToolTipText(asPinTooltip)
						.Content()
						[
							SNew(STextBlock)
							.Text(asPinText)
							.Font(detailBuilder.GetDetailFont())
						]
					]
					+ SHorizontalBox::Slot()
						.FillWidth(1)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(FMargin(4, 0, 0, 0))
						[
							nameWidget.ToSharedRef()
						];

				using FSetVisibility = TAttribute<EVisibility>;
				valueWidget->SetVisibility(FSetVisibility::Create(FSetVisibility::FGetter::CreateLambda([node, &pinProperty]() { return node->IsPinVisible(pinProperty) ? EVisibility::Collapsed : EVisibility::Visible; })));

				auto isResetToDefaultVisible = FIsResetToDefaultVisible::CreateLambda(
					[node, &pinProperty](TSharedPtr<IPropertyHandle> prop)
					{
						return !node->IsPinVisible(pinProperty) && prop->DiffersFromDefault();
					});
				auto resetToDefaultHandler = FResetToDefaultHandler::CreateLambda(
					[](TSharedPtr<IPropertyHandle> prop)
					{
						prop->ResetToDefault();
					});
				propertyRow.OverrideResetToDefault(FResetToDefaultOverride::Create(isResetToDefaultVisible, resetToDefaultHandler));

				propertyRow.CustomWidget(false)
					.NameContent()
					.MinDesiredWidth(row.NameWidget.MinWidth)
					.MaxDesiredWidth(row.NameWidget.MaxWidth)
					[
						nameWidget.ToSharedRef()
					]
					.ValueContent()
					.MinDesiredWidth(row.ValueWidget.MinWidth)
					.MaxDesiredWidth(row.ValueWidget.MaxWidth)
					[
						valueWidget.ToSharedRef()
					];
			}
		}
	}
};

TSharedRef<IDetailCustomization> IGCFSMDetailCustomization::MakeInstance()
{
	return MakeShareable(new FGCFSMDetailCustomization());
}

void IGCFSMDetailCustomization::Initialize()
{
	if (node == nullptr)
	{
		node = Cast<UK2Node>(this);

		for (TFieldIterator<FProperty> it(node->GetClass()); it; ++it)
		{
			FProperty* property = *it;
			if (property->HasMetaData(TEXT("GCFSM_VisiblePins")))
			{
				using Type = TProperty<uint32, FNumericProperty>;
				visiblePins = CastField<Type>(property);
			}
			else if (property->HasMetaData(TEXT("GCFSM_KeepOrder")))
			{
				pinProperties.Add({ property, {}, {} });
			}
			else
			{
				auto pinName = property->GetMetaData(TEXT("GCFSM_SyncWithPin"));
				if (pinName.Len())
				{
					if (auto pin = node->FindPin(pinName))
					{
						pinProperties.Add({ property, pinName, pin->PinToolTip });
					}
				}
			}
		}
		check(visiblePins);
	}
}

const IGCFSMDetailCustomization::PinProperty* IGCFSMDetailCustomization::GetPinProperty(const FString& pinName)
{
	Initialize();

	for (auto& pinProperty : pinProperties)
	{
		if (pinProperty.pinName == pinName)
		{
			return &pinProperty;
		}
	}
	return nullptr;
}

const IGCFSMDetailCustomization::PinProperty* IGCFSMDetailCustomization::GetPinProperty(FProperty* property)
{
	Initialize();

	for (auto& pinProperty : pinProperties)
	{
		if (pinProperty.property == property)
		{
			return &pinProperty;
		}
	}
	return nullptr;
}

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION < 1
static void GetPropertyText_InContainer(UObject* container, FProperty* property, FString& value)
{
	value.Empty(); // ExportTextItem appends, so we need to clear value first
	void* address = property->ContainerPtrToValuePtr<void>(container);
	property->ExportTextItem(value, address, nullptr, nullptr, 0);
}

static void SetPropertyText_InContainer(UObject* container, FProperty* property, const FString& str)
{
	void* address = property->ContainerPtrToValuePtr<void>(container);
	property->ImportText(*str, address, 0, nullptr);
}
#else
static void GetPropertyText_InContainer(UObject* container, FProperty* property, FString& value)
{
	value.Empty(); // ExportTextItem appends, so we need to clear value first
	property->ExportTextItem_InContainer(value, container, nullptr, nullptr, 0);
}

static void SetPropertyText_InContainer(UObject* container, FProperty* property, const FString& str)
{
	property->ImportText_InContainer(*str, container, nullptr, 0);
}
#endif

void IGCFSMDetailCustomization::SyncPin(const PinProperty& pinProperty)
{
	if (auto pin = node->FindPin(pinProperty.pinName))
	{
		pin->Modify();
		GetPropertyText_InContainer(node, pinProperty.property, pin->DefaultValue);
		pin->bHidden = !IsPinVisible(pinProperty) || !IsPinPropertyAvailable(pinProperty);
		if (pin->bHidden)
		{
			pin->BreakAllPinLinks();
		}
	}
}

void IGCFSMDetailCustomization::SyncProperty(const PinProperty& pinProperty)
{
	if (auto pin = node->FindPin(pinProperty.pinName))
	{
		SetPropertyText_InContainer(node, pinProperty.property, pin->DefaultValue);
	}
}

void IGCFSMDetailCustomization::SyncAllPins()
{
	Initialize();

	for (auto& pinProperty : pinProperties)
	{
		if (pinProperty.pinName.Len())
		{
			SyncPin(pinProperty);
		}
	}
}

void IGCFSMDetailCustomization::SyncAllProperties()
{
	Initialize();
	node->Modify();
	for (auto& pinProperty : pinProperties)
	{
		if (pinProperty.pinName.Len())
		{
			SyncProperty(pinProperty);
		}
	}
}

uint32 IGCFSMDetailCustomization::MaskForProperty(const PinProperty& pinProperty)
{
	return 1u << (&pinProperty - pinProperties.GetData());
}

void IGCFSMDetailCustomization::SetPinVisible(const PinProperty& pinProperty, bool visible)
{
	uint32 value = visiblePins->GetPropertyValue_InContainer(node);
	uint32 mask = MaskForProperty(pinProperty);
	uint32 newValue = visible ? (value | mask) : (value & ~mask);
	if (value != newValue)
	{
		node->Modify();
		visiblePins->SetPropertyValue_InContainer(node, newValue);
		node->ReconstructNode();
	}
}

bool IGCFSMDetailCustomization::IsPinVisible(const PinProperty& pinProperty)
{
	uint32 value = visiblePins->GetPropertyValue_InContainer(node);
	uint32 mask = MaskForProperty(pinProperty);
	return (value & mask) != 0;
}

void IGCFSMDetailCustomization::SyncPropertyWithPin(UEdGraphPin* pin)
{
	if (auto prop = GetPinProperty(pin->GetName()))
	{
		SyncProperty(*prop);
	}
}

void IGCFSMDetailCustomization::SyncPinWithProperty(UEdGraphPin* pin)
{
	if (auto prop = GetPinProperty(pin->GetName()))
	{
		SyncPin(*prop);
	}
}

void IGCFSMDetailCustomization::SyncPinWithProperty(FProperty* property)
{
	if (auto prop = GetPinProperty(property))
	{
		SyncPin(*prop);
	}
}

bool IGCFSMDetailCustomization::IsPinVisible(UEdGraphPin* pin)
{
	auto prop = GetPinProperty(pin->GetName());
	return prop ? IsPinVisible(*prop) : false;
}

#undef LOCTEXT_NAMESPACE
