#include "LayoutTemplate/UILayoutTemplate.h"

#include "LayoutTemplate/LayoutTemplatesSorter.h"
#include "LayoutTemplate/WidgetPlaceholder.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "HFSMState.h"

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
		IWidgetPlaceholder::Execute_MountWidget(*Sample, Widget);
	}
}

void UUILayoutTemplate::DemountWidget(UUserWidget* Widget, FGameplayTag PlaceholderID)
{
	if (UWidget** Placeholder = Placeholders.Find(PlaceholderID))
	{
		FWidgetDemountData DemountData;
		DemountData.StateName = State ? State->GetName() : TEXT("<none>");
		DemountData.PlaceholderID = PlaceholderID;
		DemountData.Placeholder = *Placeholder;
		DemountData.Widget = Widget;

		if (TemplateWidget->FadeOutDuration > 0)
		{
			WidgetsAwaitingDemount.Add(DemountData);
		}
		else
		{
			DemountWidgetsImmediate({ DemountData });
		}
	}
}

void UUILayoutTemplate::DemountWidgetsImmediate(const TArray<FWidgetDemountData>& WidgetsAwaitingDemount)
{
	for (const FWidgetDemountData& DemountData : WidgetsAwaitingDemount)
	{
		IWidgetPlaceholder::Execute_DemountWidget(DemountData.Placeholder, DemountData.Widget);
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
	TemplateWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UUILayoutTemplate::Show() const
{
	TemplateWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UUILayoutTemplate::SetOpacity(float Opacity)
{
	TemplateWidget->SetRenderOpacity(Opacity);
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

	APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	TemplateWidget = CreateWidget<ULayoutWidget>(LocalPlayerController, Template);
	TemplateWidget->AddToViewport(ZOrder);
	TemplateWidget->DoFadeIn();
	TemplateWidget->WidgetTree->ForEachWidget(
		[this](UWidget* Widget)
		{
			if (Widget->Implements<UWidgetPlaceholder>())
			{
				const IWidgetPlaceholder* Placeholder = Cast<IWidgetPlaceholder>(Widget);
				const FGameplayTag ID = Placeholder->Execute_GetID(Widget);
				Placeholders.Add(ID, Widget);
			}
		});

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

	if (TemplateWidget->FadeOutDuration > 0)
	{
		TemplateWidget->DoFadeOut();
		FTimerHandle DestroyTimer;
		const auto Delegate = FTimerDelegate::CreateLambda(
			[DemountList = WidgetsAwaitingDemount, Widget = TemplateWidget]()
			{
				UUILayoutTemplate::DemountWidgetsImmediate(DemountList);
				Widget->RemoveFromParent();
			});
		GetWorld()->GetTimerManager().SetTimer(DestroyTimer, Delegate, TemplateWidget->FadeOutDuration, false);
	}
	else
	{
		DemountWidgetsImmediate(WidgetsAwaitingDemount);
		TemplateWidget->RemoveFromParent();
	}
	Super::Exit(event);
}
