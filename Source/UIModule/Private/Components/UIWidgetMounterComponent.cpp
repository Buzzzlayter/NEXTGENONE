#include "Components/UIWidgetMounterComponent.h"

#include "Blueprint/UserWidget.h"
#include "HFSMState.h"
#include "Kismet/GameplayStatics.h"
#include "LayoutTemplate/LayoutTemplatesSorter.h"
#include "LayoutTemplate/UILayoutTemplate.h"
#include "UIModule.h"

void UUIWidgetMounterComponent::Enter()
{
	GENCHECK(WidgetClass, "UUIWidgetMounterComponent::Enter failed. WidgetClass is null.");

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	GENCHECK(PlayerController, "UUIWidgetMounterComponent::Enter failed. PlayerController is null.");

	Widget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
	GENCHECK(Widget, "UUIWidgetMounterComponent::Enter failed. CreateWidget returned null.");

	MountWidget();

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
	if (!GetState())
	{
		return nullptr;
	}

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

void UUIWidgetMounterComponent::MountWidget()
{
	if (!Widget)
	{
		return;
	}

	MountedLayout = ResolveLayout();
	if (MountedLayout && PlaceholderID.IsValid())
	{
		if (!MountedLayout->GetPlaceholders().Contains(PlaceholderID))
		{
			UE_LOG(LogTemp,
				Warning,
				TEXT("Widget Mounter did not find placeholder '%s' in layout '%s'."),
				*PlaceholderID.ToString(),
				*GetNameSafe(MountedLayout));
		}

		MountedLayout->MountWidgetTo(Widget, PlaceholderID);
		return;
	}

	if (bAddToViewportIfNoLayout)
	{
		Widget->AddToViewport(FallbackZOrder);
		return;
	}

	UE_LOG(LogTemp,
		Warning,
		TEXT("Widget Mounter could not mount '%s': layout=%s placeholder='%s'."),
		*GetNameSafe(Widget),
		*GetNameSafe(MountedLayout),
		*PlaceholderID.ToString());
}

void UUIWidgetMounterComponent::DemountWidget()
{
	if (!Widget)
	{
		return;
	}

	if (MountedLayout && PlaceholderID.IsValid())
	{
		MountedLayout->DemountWidget(Widget, PlaceholderID);
	}
	else
	{
		Widget->RemoveFromParent();
	}

	Widget = nullptr;
	MountedLayout = nullptr;
}
