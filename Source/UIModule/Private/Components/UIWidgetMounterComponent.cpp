#include "Components/UIWidgetMounterComponent.h"

#include "Blueprint/UserWidget.h"
#include "HFSMState.h"
#include "GameFramework/PlayerController.h"
#include "LayoutTemplate/LayoutTemplatesSorter.h"
#include "LayoutTemplate/UILayoutTemplate.h"
#include "LayoutTemplate/WidgetPlaceholder.h"

void UUIWidgetMounterComponent::Enter()
{
	ResetBlueprintInput();
	DemountWidget(true);
	if (!ensureMsgf(GetState(), TEXT("Widget Mounter has no owning state."))) return;
	if (!ensureMsgf(WidgetClass, TEXT("Widget Mounter has no widget class."))) return;
	if (!ensureMsgf(PlaceholderID.IsValid(), TEXT("Widget Mounter has no PlaceholderID."))) return;
	
	APlayerController* PlayerController = GetPlayerController();
	if (!ensureMsgf(IsValid(PlayerController) && PlayerController->GetLocalPlayer(),
		TEXT("Widget Mounter requires an explicit local player context."))) return;
	
	Widget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
	if (!ensureMsgf(Widget, TEXT("Widget Mounter could not create its widget."))) return;
	
	if (!MountWidget())
	{
		DemountWidget(true);
		return;
	}
	Super::Enter();
}

void UUIWidgetMounterComponent::Exit(FName Event)
{
	Super::Exit(Event);
	DemountWidget();
}

UUserWidget* UUIWidgetMounterComponent::GetWidget() const
{
	return Widget;
}

UUILayoutTemplate* UUIWidgetMounterComponent::ResolveLayout() const
{
	UHFSMStateComponent* SorterComponent = nullptr;
	GetState()->GetClosestComponentInHierarchyUp(ULayoutTemplatesSorter::StaticClass(), SorterComponent);

	if (ULayoutTemplatesSorter* Sorter = Cast<ULayoutTemplatesSorter>(SorterComponent))
	{
		if (UUILayoutTemplate* Layout = Sorter->GetLayer(Layer))
		{
			return Layout;
		}
	}
	
	UHFSMStateComponent* LayoutComponent = nullptr;
	GetState()->GetClosestComponentInHierarchyUp(UUILayoutTemplate::StaticClass(), LayoutComponent);
	return Cast<UUILayoutTemplate>(LayoutComponent);
}

bool UUIWidgetMounterComponent::MountWidget()
{
	if (!Widget) return false;
	
	MountedLayout = ResolveLayout();
	if (MountedLayout && PlaceholderID.IsValid())
	{
		if (!ensureMsgf(MountedLayout->GetPlaceholders().Contains(PlaceholderID),
			TEXT("Widget Mounter could not find its placeholder.")))
		{
			return false;
		}

		if (!MountedLayout->MountWidgetTo(Widget, PlaceholderID)) return false;
		MountedPlaceholderID = PlaceholderID;
		return true;
	}
	
	ensureMsgf(false, TEXT("Widget Mounter has no layout target or viewport fallback."));
	return false;
}

void UUIWidgetMounterComponent::DemountWidget(bool bImmediate)
{
	if (!Widget) { MountedLayout = nullptr; MountedPlaceholderID = {}; return; }
	Widget->SetIsEnabled(false); // Deferred fade must not keep an exited state's controls interactive.
	
	if (MountedLayout && MountedPlaceholderID.IsValid())
	{
		if (bImmediate)
		{
			if (UWidget* const* Placeholder = MountedLayout->GetPlaceholders().Find(MountedPlaceholderID))
			{
				if (*Placeholder)
				{
					IWidgetPlaceholder::Execute_DemountWidget(*Placeholder, Widget);
				}
			}
			Widget->RemoveFromParent();
		}
		else
		{
			MountedLayout->DemountWidget(Widget, MountedPlaceholderID);
		}
	}
	else
	{
		Widget->RemoveFromParent();
	}

	Widget = nullptr;
	MountedLayout = nullptr;
	MountedPlaceholderID = {};
}
