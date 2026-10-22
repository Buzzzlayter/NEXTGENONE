#include "LayoutTemplate/UILayoutTemplate.h"

#include "LayoutTemplate/LayoutTemplatesSorter.h"
#include "LayoutTemplate/WidgetPlaceholder.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HFSMState.h"
#include "TimerManager.h"

ULayoutTemplatesSorter* GetLayoutSorter(UHFSMState* State)
{
	if (!State)
	{
		return nullptr;
	}
	UHFSMStateComponent* Component = nullptr;
	State->GetClosestComponentInHierarchyUp(ULayoutTemplatesSorter::StaticClass(), Component);
	if (Component)
		return Cast<ULayoutTemplatesSorter>(Component);
	return nullptr;
}

bool UUILayoutTemplate::MountWidgetTo(UUserWidget* Widget, FGameplayTag PlaceholderID)
{
	if (UWidget** Sample = Placeholders.Find(PlaceholderID))
	{
		if (*Sample)
		{
			return ensureMsgf(IWidgetPlaceholder::Execute_MountWidget(*Sample, Widget),
				TEXT("Layout placeholder rejected the widget."));
		}
	}
	return false;
}

void UUILayoutTemplate::DemountWidget(UUserWidget* Widget, FGameplayTag PlaceholderID)
{
	if (UWidget** Placeholder = Placeholders.Find(PlaceholderID))
	{
		if (!*Placeholder)
		{
			return;
		}

		FUIModuleWidgetDemountData DemountData;
		DemountData.StateName = State ? State->GetName() : TEXT("<none>");
		DemountData.PlaceholderID = PlaceholderID;
		DemountData.Placeholder = *Placeholder;
		DemountData.Widget = Widget;

		if (TemplateWidget && TemplateWidget->FadeOutDuration > 0)
		{
			WidgetsAwaitingDemount.Add(DemountData);
		}
		else
		{
			DemountWidgetsImmediate({ DemountData });
		}
	}
}

void UUILayoutTemplate::DemountWidgetsImmediate(const TArray<FUIModuleWidgetDemountData>& WidgetsAwaitingDemount)
{
	for (const FUIModuleWidgetDemountData& DemountData : WidgetsAwaitingDemount)
	{
		UWidget* Placeholder = DemountData.Placeholder.Get();
		UUserWidget* Widget = DemountData.Widget.Get();
		if (Placeholder && Widget)
		{
			IWidgetPlaceholder::Execute_DemountWidget(Placeholder, Widget);
		}
	}
}

void UUILayoutTemplate::LayersUpdate(bool& bOutConsumeDrawing)
{
	if (!ensureMsgf(TemplateWidget, TEXT("Layout has no widget when updating layers.")))
	{
		return;
	}
	bOutConsumeDrawing |= TemplateWidget->bConsumeLoverLayers;
}

void UUILayoutTemplate::SetVisible(bool isVisible) const
{
	if (TemplateWidget)
	{
		TemplateWidget->SetVisibility(isVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UUILayoutTemplate::SetOpacity(float Opacity)
{
	if (TemplateWidget)
	{
		TemplateWidget->SetRenderOpacity(Opacity);
	}
}

bool UUILayoutTemplate::IsLayer(const ELayoutLayer& InLayer) const
{
	return Layer == InLayer;
}

bool UUILayoutTemplate::operator<(const UUILayoutTemplate& Other) const
{
	return Layer < Other.Layer;
}

const TMap<FGameplayTag, UWidget*>& UUILayoutTemplate::GetPlaceholders()
{
	return Placeholders;
}

bool UUILayoutTemplate::Enter()
{
	if (!ensureMsgf(Template, TEXT("Layout has no template class.")))
	{
		return false;
	}

	UWorld* world = GetWorld();
	if (!ensureMsgf(world, TEXT("Layout has no world.")))
	{
		return false;
	}

	APlayerController* LocalPlayerController = GetPlayerController();
	if (!ensureMsgf(IsValid(LocalPlayerController) && LocalPlayerController->GetLocalPlayer(),
		TEXT("Layout requires an explicit local player context.")))
	{
		return false;
	}

	TemplateWidget = CreateWidget<ULayoutWidget>(LocalPlayerController, Template);
	if (!ensureMsgf(TemplateWidget, TEXT("Layout could not create its widget.")))
	{
		return false;
	}

	if (!ensureMsgf(TemplateWidget->AddToPlayerScreen(static_cast<int32>(Layer)),
		TEXT("Layout could not add its widget to the owning player's screen.")))
	{
		TemplateWidget = nullptr;
		return false;
	}
	TemplateWidget->DoFadeIn();
	if (TemplateWidget->WidgetTree)
	{
		TemplateWidget->WidgetTree->ForEachWidget(
			[this](UWidget* Widget)
			{
				if (Widget && Widget->Implements<UWidgetPlaceholder>())
				{
					const FGameplayTag ID = IWidgetPlaceholder::Execute_GetID(Widget);
					if (ID.IsValid())
					{
						Placeholders.Add(ID, Widget);
					}
				}
			});
	}

	if (!Super::Enter())
	{
		TemplateWidget->RemoveFromParent();
		TemplateWidget = nullptr;
		Placeholders.Reset();
		return false;
	}
	bIsTickEnabled = true;
	
	if (ULayoutTemplatesSorter* LayoutSorter = GetLayoutSorter(State))
		LayoutSorter->AddLayout(this);
	return true;
}

void UUILayoutTemplate::AbortEnter()
{
	if (ULayoutTemplatesSorter* LayoutSorter = GetLayoutSorter(State))
	{
		LayoutSorter->RemoveLayout(this);
	}
	DemountWidgetsImmediate(WidgetsAwaitingDemount);
	WidgetsAwaitingDemount.Reset();
	if (TemplateWidget)
	{
		TemplateWidget->RemoveFromParent();
		TemplateWidget = nullptr;
	}
	Placeholders.Reset();
	bIsTickEnabled = false;
	Super::Exit(NAME_None);
}

void UUILayoutTemplate::Tick(float deltaTime)
{
	if (WidgetsAwaitingDemount.Num() > 0)
	{
		DemountWidgetsImmediate(WidgetsAwaitingDemount);
		WidgetsAwaitingDemount.Reset();
	}

	Super::Tick(deltaTime);
}

void UUILayoutTemplate::Exit(FName event)
{
	if (const auto LayoutSorter = GetLayoutSorter(State))
		LayoutSorter->RemoveLayout(this);

	if (!TemplateWidget)
	{
		Super::Exit(event);
		return;
	}
	
	if (TemplateWidget->FadeOutDuration > 0)
	{
		TemplateWidget->DoFadeOut();
		FTimerHandle DestroyTimer;
		const TArray<FUIModuleWidgetDemountData> DemountList = WidgetsAwaitingDemount;
		WidgetsAwaitingDemount.Reset();
		TWeakObjectPtr<ULayoutWidget> TemplateWidgetWeak = TemplateWidget;
		const auto Delegate = FTimerDelegate::CreateLambda(
			[DemountList, TemplateWidgetWeak]()
			{
				UUILayoutTemplate::DemountWidgetsImmediate(DemountList);
				if (ULayoutWidget* Widget = TemplateWidgetWeak.Get())
				{
					Widget->RemoveFromParent();
				}
			});
		if (UWorld* world = GetWorld())
		{
			world->GetTimerManager().SetTimer(DestroyTimer, Delegate, TemplateWidget->FadeOutDuration, false);
		}
		else
		{
			DemountWidgetsImmediate(DemountList);
			TemplateWidget->RemoveFromParent();
		}
	}
	else
	{
		DemountWidgetsImmediate(WidgetsAwaitingDemount);
		WidgetsAwaitingDemount.Reset();
		TemplateWidget->RemoveFromParent();
	}
	Super::Exit(event);
}
