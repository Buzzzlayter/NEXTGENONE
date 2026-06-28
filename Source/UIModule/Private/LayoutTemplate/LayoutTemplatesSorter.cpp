#include "LayoutTemplate/LayoutTemplatesSorter.h"

#include "LayoutTemplate/UILayoutTemplate.h"
#include "LayoutTemplate/WidgetPlaceholder.h"


void ULayoutTemplatesSorter::SetLayoutsVisibility(bool IsVisible)
{
	bLayoutsVisible = IsVisible;
	Update();
}

void ULayoutTemplatesSorter::SetCustomVisibility(bool bIsEnable, const FGameplayTag& HidePresetTag)
{
	for (UUILayoutTemplate* Template : TemplatesList)
	{
		for (const TPair<FGameplayTag, UWidget*>& InPlaceholder : Template->GetPlaceholders())
		{
			IWidgetPlaceholder::Execute_SendVisibility(InPlaceholder.Value, bIsEnable, HidePresetTag);
		}
	}
}

UUILayoutTemplate* ULayoutTemplatesSorter::GetLayer(const ELayoutLayer Layer)
{
	for (int32 Index = TemplatesList.Num() - 1; Index >= 0; --Index)
	{
		UUILayoutTemplate* Template = TemplatesList[Index];
		if (Template->IsLayer(Layer))
		{
			return Template;
		}
	}

	return nullptr;
}

void ULayoutTemplatesSorter::AddLayout(UUILayoutTemplate* LayoutTemplate)
{
	TemplatesList.Add(LayoutTemplate);
	Update();
}

void ULayoutTemplatesSorter::RemoveLayout(UUILayoutTemplate* LayoutTemplate)
{
	TemplatesList.Remove(LayoutTemplate);
	Update();
}

void ULayoutTemplatesSorter::Update() const
{
	bool bConsumeDraw = !bLayoutsVisible;

	auto TmpTemplatesList = TemplatesList;
	TmpTemplatesList.Sort();

	auto Iter = TmpTemplatesList.end();
	auto Index = TmpTemplatesList.Num();
	while (Iter != TmpTemplatesList.begin())
	{
		const auto Template = *--Iter;
		if (bConsumeDraw)
		{
			Template->Hide();
		}
		else
		{
			Template->Show();
			Template->LayersUpdate(--Index, bConsumeDraw);
		}
	}
}
