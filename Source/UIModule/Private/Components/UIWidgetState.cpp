#include "Components/UIWidgetState.h"

#include "Components/UIWidgetMounterComponent.h"

UUIWidgetState::UUIWidgetState()
{
	WidgetMounter = CreateDefaultSubobject<UUIWidgetMounterComponent>(TEXT("WidgetMounter"));
}

void UUIWidgetState::Enter()
{
	WidgetMounter->Init(this);
	WidgetMounter->SetMountTarget(WidgetClass, PlaceholderID, Layer);
	WidgetMounter->Enter();
	Super::Enter();
}

void UUIWidgetState::Exit(FName Event)
{
	WidgetMounter->Exit(Event);
	Super::Exit(Event);
}
