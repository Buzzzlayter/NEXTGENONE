#include "Components/UIWidgetMounterComponent.h"

#include "Blueprint/UserWidget.h"
#include "HFSMState.h"
#include "GameFramework/PlayerController.h"
#include "LayoutTemplate/LayoutTemplatesSorter.h"
#include "LayoutTemplate/UILayoutTemplate.h"
#include "LayoutTemplate/WidgetPlaceholder.h"

bool UUIWidgetMounterComponent::Enter()
{
	if (!ensureMsgf(GetState(), TEXT("Widget Mounter has no owning state.")))
	{
		return false;
	}
	if (!ensureMsgf(WidgetClass, TEXT("Widget Mounter has no widget class.")))
	{
		return false;
	}

	APlayerController* PlayerController = GetPlayerController();
	if (!ensureMsgf(IsValid(PlayerController) && PlayerController->GetLocalPlayer(),
		TEXT("Widget Mounter requires an explicit local player context.")))
	{
		return false;
	}

	Widget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
	if (!ensureMsgf(Widget, TEXT("Widget Mounter could not create its widget.")))
	{
		return false;
	}
	if (!MountWidget() || !Super::Enter())
	{
		DemountWidget(true);
		return false;
	}
	return true;
}

void UUIWidgetMounterComponent::Exit(FName Event)
{
	Super::Exit(Event);
	DemountWidget();
}

void UUIWidgetMounterComponent::AbortEnter()
{
	Super::Exit(NAME_None);
	DemountWidget(true);
}

UUserWidget* UUIWidgetMounterComponent::GetWidget() const
{
	return Widget;
}

UUILayoutTemplate* UUIWidgetMounterComponent::ResolveLayout() const
{
	if (bUseLayer)
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
	}

	UHFSMStateComponent* LayoutComponent = nullptr;
	GetState()->GetClosestComponentInHierarchyUp(UUILayoutTemplate::StaticClass(), LayoutComponent);
	return Cast<UUILayoutTemplate>(LayoutComponent);
}

bool UUIWidgetMounterComponent::MountWidget()
{
	if (!Widget)
	{
		return false;
	}

	MountedLayout = ResolveLayout();
	if (MountedLayout && PlaceholderID.IsValid())
	{
		if (!ensureMsgf(MountedLayout->GetPlaceholders().Contains(PlaceholderID),
			TEXT("Widget Mounter could not find its placeholder.")))
		{
			return false;
		}

		return MountedLayout->MountWidgetTo(Widget, PlaceholderID);
	}

	if (bAddToViewportIfNoLayout)
	{
		return ensureMsgf(Widget->AddToPlayerScreen(FallbackZOrder),
			TEXT("Widget Mounter could not add its widget to the owning player's screen."));
	}
	ensureMsgf(false, TEXT("Widget Mounter has no layout target or viewport fallback."));
	return false;
}

void UUIWidgetMounterComponent::DemountWidget(bool bImmediate)
{
	if (!Widget)
	{
		return;
	}

	if (MountedLayout && PlaceholderID.IsValid())
	{
		if (bImmediate)
		{
			if (UWidget* const* Placeholder = MountedLayout->GetPlaceholders().Find(PlaceholderID))
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
			MountedLayout->DemountWidget(Widget, PlaceholderID);
		}
	}
	else
	{
		Widget->RemoveFromParent();
	}

	Widget = nullptr;
	MountedLayout = nullptr;
}
