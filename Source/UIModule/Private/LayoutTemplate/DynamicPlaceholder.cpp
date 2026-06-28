#include "LayoutTemplate/DynamicPlaceholder.h"

#include "Blueprint/UserWidget.h"

FGameplayTag UDynamicPlaceholder::GetID_Implementation() const
{
	return PlaceholderTag;
}

bool UDynamicPlaceholder::MountWidget_Implementation(UUserWidget* Widget)
{
	if (!Widget)
		return false;
	
	if (GetContent() == Widget)
		return true;

	Widget->RemoveFromParent();
	SetContent(Widget);
	return true;
}

bool UDynamicPlaceholder::DemountWidget_Implementation(UUserWidget* Widget)
{
	if (!Widget)
		return false;

	if (GetContent() == Widget)
	{
		SetContent(nullptr);
		return true;
	}

	return false;
}

bool UDynamicPlaceholder::ShowWidgets_Implementation(TArray<TSubclassOf<UUserWidget>>& Widgets)
{
	if (MatchesWidgetList(Widgets))
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return true;
	}

	return false;
}

bool UDynamicPlaceholder::HideWidgets_Implementation(TArray<TSubclassOf<UUserWidget>>& Widgets)
{
	if (MatchesWidgetList(Widgets))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return true;
	}

	return false;
}

void UDynamicPlaceholder::ShowAnimation_Implementation()
{
}

void UDynamicPlaceholder::SendVisibility_Implementation(bool bCondition, const FGameplayTag& HidePesetTag)
{
	const bool bShouldHide = bCondition && HidePesetTag.IsValid() && PlaceholderTag.MatchesTagExact(HidePesetTag);
	SetVisibility(bShouldHide ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UDynamicPlaceholder::Dispatch_Implementation(FGameplayTag Event)
{
}

bool UDynamicPlaceholder::MatchesWidgetList(const TArray<TSubclassOf<UUserWidget>>& Widgets) const
{
	const UUserWidget* MountedWidget = GetMountedUserWidget();
	if (!MountedWidget)
	{
		return false;
	}

	for (const TSubclassOf<UUserWidget>& WidgetClass : Widgets)
	{
		if (WidgetClass && MountedWidget->IsA(WidgetClass))
		{
			return true;
		}
	}

	return false;
}

UUserWidget* UDynamicPlaceholder::GetMountedUserWidget() const
{
	return Cast<UUserWidget>(GetContent());
}

#if WITH_EDITOR
const FText UDynamicPlaceholder::GetPaletteCategory()
{
	return NSLOCTEXT("UIModule", "LayoutTemplateCategory", "UI Module");
}
#endif
