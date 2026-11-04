#if WITH_EDITOR

#include "HFSMComponentCustomization.h"
#include "CoreMinimal.h"
#include "HFSMStateComponent.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "SourceCodeNavigation.h"
#include "UObject/UnrealType.h"
#include "Widgets/SBoxPanel.h"

namespace
{
	const FName ComponentTypeName(TEXT("HFSMStateComponent"));

	UClass* ResolveComponentClass(const TSharedRef<IPropertyHandle>& Property)
	{
		UObject* Component = nullptr;
		return Property->GetValue(Component) == FPropertyAccess::Success && IsValid(Component)
			? Component->GetClass() : nullptr;
	}

	void BrowseComponentClass(TSharedRef<IPropertyHandle> Property)
	{
		UClass* Class = ResolveComponentClass(Property);
		if (!Class) return;
		if (!IsValid(Class->ClassGeneratedBy))
		{
			FSourceCodeNavigation::NavigateToClass(Class);
			return;
		}
		if (GEditor)
		{
			const TArray<UObject*> Assets{Class->ClassGeneratedBy};
			GEditor->SyncBrowserToObjects(Assets);
		}
	}

	bool CanBrowseComponentClass(TSharedRef<IPropertyHandle> Property)
	{
		const UClass* Class = ResolveComponentClass(Property);
		return Class && (IsValid(Class->ClassGeneratedBy) ? GEditor != nullptr : FSourceCodeNavigation::CanNavigateToClass(Class));
	}

	class FHFSMComponentCustomization : public IPropertyTypeCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> MakeInstance()
		{
			return MakeShared<FHFSMComponentCustomization>();
		}

		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Property, FDetailWidgetRow& Row,
			IPropertyTypeCustomizationUtils& Utils) override
		{
			Row.NameContent()[Property->CreatePropertyNameWidget()];
			Row.ValueContent().MinDesiredWidth(250.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[Property->CreatePropertyValueWidget()]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeBrowseButton(
						FSimpleDelegate::CreateStatic(&BrowseComponentClass, Property),
						NSLOCTEXT("HFSM", "BrowseComponentClass", "Find the component Blueprint in the Content Browser, or open its C++ source."),
						TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateStatic(&CanBrowseComponentClass, Property)))
				]
			];
		}

		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Property, IDetailChildrenBuilder& Children,
			IPropertyTypeCustomizationUtils& Utils) override
		{
			// An inline UObject has an object node, not ordinary struct child handles.
			// Edit the actual instances through standard external property rows, never their CDOs.
			const auto* ObjectProperty = CastField<FObjectPropertyBase>(Property->GetProperty());
			if (!ObjectProperty) return;
			TArray<UObject*> Components;
			UClass* CommonClass = nullptr;
			Property->EnumerateConstRawData([&](const void* Data, int32, int32)
			{
				UObject* Component = Data ? ObjectProperty->GetObjectPropertyValue(Data) : nullptr;
				if (!IsValid(Component) || !Component->IsA<UHFSMStateComponent>()) return true;
				Components.AddUnique(Component);
				CommonClass = CommonClass ? UClass::FindCommonBase(CommonClass, Component->GetClass()) : Component->GetClass();
				return true;
			});
			if (!CommonClass) return;
			for (TFieldIterator<FProperty> Field(CommonClass); Field; ++Field)
			{
				if (!Field->HasAnyPropertyFlags(CPF_Edit)
					|| Field->HasAnyPropertyFlags(CPF_Deprecated | CPF_DisableEditOnInstance)) continue;
				Children.AddExternalObjectProperty(Components, Field->GetFName(), FAddPropertyParams().AllowChildren(true));
			}
		}
	};
}

void HFSMComponentEditor::RegisterCustomization()
{
	auto& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	Module.RegisterCustomPropertyTypeLayout(ComponentTypeName,
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FHFSMComponentCustomization::MakeInstance));
	Module.NotifyCustomizationModuleChanged();
}

void HFSMComponentEditor::UnregisterCustomization()
{
	if (auto* Module = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
	{
		Module->UnregisterCustomPropertyTypeLayout(ComponentTypeName);
		Module->NotifyCustomizationModuleChanged();
	}
}

#endif
