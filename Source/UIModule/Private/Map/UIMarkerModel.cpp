#include "Map/UIMarkerModel.h"

#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Map/UIMarkerWidget.h"
#include "Net/UnrealNetwork.h"

UUIMarkerModel* UUIMarkerModel::CreateRuntimeModel(UObject* Outer, UUIMarkerModel* Template, const FVector& Position, FName RecipientGroup, AActor* Source)
{
	if (!IsValid(Outer) || !IsValid(Template)) return nullptr;
	auto* Model = NewObject<UUIMarkerModel>(Outer, Template->GetClass(), NAME_None, RF_Transient, Template);
	Model->MarkerId = FGuid::NewGuid();
	Model->WorldPosition = FVector_NetQuantize10(Position);
	Model->RecipientGroup = RecipientGroup;
	Model->SourceActor = Source;
	return Model;
}

void UUIMarkerModel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, MarkerId);
	DOREPLIFETIME(ThisClass, WorldPosition);
	DOREPLIFETIME(ThisClass, RecipientGroup);
	DOREPLIFETIME(ThisClass, Label);
	DOREPLIFETIME(ThisClass, Icon);
	DOREPLIFETIME(ThisClass, WidgetClass);
	DOREPLIFETIME(ThisClass, WorldWidgetClass);
	DOREPLIFETIME(ThisClass, MapVisibility);
	DOREPLIFETIME(ThisClass, WorldVisibility);
}

bool UUIMarkerModel::IsVisibleTo(const APlayerController& Viewer) const
{
	return RecipientGroup.IsNone() || !Viewer.HasAuthority() || Viewer.IsMemberOfNetConditionGroup(RecipientGroup);
}

void UUIMarkerModel::NotifyMarkerChanged()
{
	MarkerChanged.Broadcast(this, true);
}

void UUIMarkerModel::SetPosition(const FVector& Position)
{
	if (Position.ContainsNaN() || WorldPosition == Position) return;
	WorldPosition = FVector_NetQuantize10(Position);
	OnRep_WorldPosition();
}

void UUIMarkerModel::OnRep_WorldPosition()
{
	MarkerChanged.Broadcast(this, false);
}

UTexture2D* UUIMarkerModel::GetIcon() const
{
	return Icon.Get();
}

const FUIMarkerVisibilityRules* UUIMarkerModel::GetVisibility(EUIMarkerSurface Surface) const
{
	switch (Surface)
	{
	case EUIMarkerSurface::Map:
		return &MapVisibility;
	case EUIMarkerSurface::Minimap:
		return &MapVisibility;
	case EUIMarkerSurface::World:
		return &WorldVisibility;
	default:
		return nullptr;
	}
}

UClass* UUIMarkerModel::GetWidgetClass(EUIMarkerSurface Surface) const
{
	if (Surface == EUIMarkerSurface::World && WorldWidgetClass)
		return WorldWidgetClass.Get();

	auto* MarkerClass = WidgetClass.Get();
	return MarkerClass ? MarkerClass : UUIMarkerWidget::StaticClass();
}
