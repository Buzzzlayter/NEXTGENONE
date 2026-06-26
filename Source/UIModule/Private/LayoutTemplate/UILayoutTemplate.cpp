#include "LayoutTemplate/UILayoutTemplate.h"

#include "LayoutTemplate/LayoutTemplatesSorter.h"
#include "LayoutTemplate/WidgetPlaceholder.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "HFSMState.h"
#include "UIModule.h"

ULayoutTemplatesSorter* GetLayoutSorter(UHFSMState* State)
{
	UHFSMStateComponent* Component = nullptr;
	State->GetClosestComponentInHierarchyUp(ULayoutTemplatesSorter::StaticClass(), Component);
	if (Component)
		return Cast<ULayoutTemplatesSorter>(Component);
	return nullptr;
}

void UUILayoutTemplate::MountWidgetTo(UUserWidget* Widget, FGameplayTag PlaceholderID)
{
	if (UWidget** Sample = Placeholders.Find(PlaceholderID))
	{
		if (*Sample)
		{
			IWidgetPlaceholder::Execute_MountWidget(*Sample, Widget);
		}
	}
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

void UUILayoutTemplate::LayersUpdate(const int32 Index, bool& bOutConsumeDrawing)
{
	if (Index != ZOrder)
	{
		ZOrder = Index;
		//Take widget for prevent SWidget destruction while we changing ZOrder
		TSharedPtr<SWidget> Capture = TemplateWidget->TakeWidget();
		TemplateWidget->RemoveFromParent();
		TemplateWidget->AddToViewport(ZOrder);
	}

	bOutConsumeDrawing |= TemplateWidget->bConsumeLoverLayers;
}

void UUILayoutTemplate::Hide() const
{
	if (TemplateWidget)
	{
		TemplateWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UUILayoutTemplate::Show() const
{
	if (TemplateWidget)
	{
		TemplateWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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

void UUILayoutTemplate::Enter()
{
	bIsTickEnabled = true;

	GENCHECK(Template, "UILayoutTemplate::Enter failed. Template is null.");

	UWorld* world = GetWorld();
	GENCHECK(world, "UILayoutTemplate::Enter failed. World is null.");

	APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(world, 0);
	GENCHECK(LocalPlayerController, "UILayoutTemplate::Enter failed. PlayerController is null.");

	TemplateWidget = CreateWidget<ULayoutWidget>(LocalPlayerController, Template);
	GENCHECK(TemplateWidget, "UILayoutTemplate::Enter failed. CreateWidget returned null.");

	TemplateWidget->AddToViewport(ZOrder);
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

	Super::Enter();
	
	if (ULayoutTemplatesSorter* LayoutSorter = GetLayoutSorter(State))
		LayoutSorter->AddLayout(this);
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
