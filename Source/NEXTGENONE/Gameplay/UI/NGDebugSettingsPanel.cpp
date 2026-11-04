#include "Gameplay/UI/NGDebugSettingsPanel.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Components/ALSDebugComponent.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UNGDebugSettingsPanel::RebuildWidget()
{
	if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Body=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Body;
		auto* Label=WidgetTree->ConstructWidget<UTextBlock>(); Label->SetAutoWrapText(true); Label->SetText(FText::FromString(TEXT("ОТЛАДКА ALS · переключатели применяются сразу, для текущего запуска"))); Body->AddChild(Label);
		auto Check=[&](const TCHAR* Name,const TCHAR* Text)
		{
			auto* Box=WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),Name); auto* TextWidget=WidgetTree->ConstructWidget<UTextBlock>(); TextWidget->SetText(FText::FromString(Text)); Box->SetContent(TextWidget); Body->AddChild(Box); return Box;
		};
		Check(TEXT("DebugView"),TEXT("Отладочный ракурс камеры"))->OnCheckStateChanged.AddDynamic(this,&ThisClass::ViewChanged);
		Check(TEXT("DebugTraces"),TEXT("Показывать трассы ALS"))->OnCheckStateChanged.AddDynamic(this,&ThisClass::TracesChanged);
		Check(TEXT("DebugShapes"),TEXT("Показывать формы отладочных коллизий"))->OnCheckStateChanged.AddDynamic(this,&ThisClass::ShapesChanged);
		Check(TEXT("DebugLayers"),TEXT("Показывать цвета анимационных слоёв"))->OnCheckStateChanged.AddDynamic(this,&ThisClass::LayersChanged);
		Info=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("CharacterInfo")); Info->SetAutoWrapText(true); Body->AddChild(Info);
	}
	View=Cast<UCheckBox>(WidgetTree->FindWidget(TEXT("DebugView"))); Traces=Cast<UCheckBox>(WidgetTree->FindWidget(TEXT("DebugTraces")));
	Shapes=Cast<UCheckBox>(WidgetTree->FindWidget(TEXT("DebugShapes"))); Layers=Cast<UCheckBox>(WidgetTree->FindWidget(TEXT("DebugLayers"))); Info=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CharacterInfo")));
	return Super::RebuildWidget();
}
UALSDebugComponent* UNGDebugSettingsPanel::GetDebug() const
{
	const auto* PC=GetOwningPlayer(); const APawn* Pawn=PC ? PC->GetPawn() : nullptr; return Pawn ? Pawn->FindComponentByClass<UALSDebugComponent>() : nullptr;
}
void UNGDebugSettingsPanel::NativeConstruct() { Super::NativeConstruct(); Refresh(); }
void UNGDebugSettingsPanel::NativeTick(const FGeometry& Geometry,float Delta)
{
	Super::NativeTick(Geometry,Delta); RefreshDelay-=Delta; if (RefreshDelay<=0.f) { RefreshDelay=.25f; Refresh(); }
}
void UNGDebugSettingsPanel::Refresh()
{
	auto* Debug=GetDebug();
	if (View) { View->SetIsEnabled(Debug!=nullptr); View->SetIsChecked(Debug && Debug->GetDebugView()); }
	if (Traces) { Traces->SetIsEnabled(Debug!=nullptr); Traces->SetIsChecked(Debug && Debug->GetShowTraces()); }
	if (Shapes) { Shapes->SetIsEnabled(Debug!=nullptr); Shapes->SetIsChecked(Debug && Debug->GetShowDebugShapes()); }
	if (Layers) { Layers->SetIsEnabled(Debug!=nullptr); Layers->SetIsChecked(Debug && Debug->GetShowLayerColors()); }
	if (!Info) return;
	const auto* Pawn=GetOwningPlayer() ? Cast<ANextgenoneCharacter>(GetOwningPlayer()->GetPawn()) : nullptr;
	if (!Pawn) { Info->SetText(FText::FromString(TEXT("Персонаж не создан."))); return; }
	const TCHAR* State=TEXT("Другое");
	switch (Pawn->GetMovementState())
	{
		case EALSMovementState::Grounded: State=TEXT("На земле"); break;
		case EALSMovementState::InAir: State=TEXT("В воздухе"); break;
		case EALSMovementState::Mantling: State=TEXT("Залезание"); break;
		case EALSMovementState::Ragdoll: State=TEXT("Ragdoll"); break;
		default: break;
	}
	const auto* Weapon=Pawn->Combat->GetEquipment().Definition.Get();
	Info->SetText(FText::FromString(FString::Printf(TEXT("%s · HP %.0f / %.0f · скорость %.0f см/с\nОружие: %s · магазин %d · запас %d"),State,Pawn->GetHealth(),Pawn->GetMaxHealth(),Pawn->GetVelocity().Size(),*GetNameSafe(Weapon),Pawn->Combat->GetMagazineAmmo(),Pawn->Combat->GetReserveAmmo())));
}
void UNGDebugSettingsPanel::ViewChanged(bool bValue) { if (auto* D=GetDebug()) if (D->GetDebugView()!=bValue) D->ToggleDebugView(); }
void UNGDebugSettingsPanel::TracesChanged(bool bValue) { if (auto* D=GetDebug()) if (D->GetShowTraces()!=bValue) D->ToggleTraces(); }
void UNGDebugSettingsPanel::ShapesChanged(bool bValue) { if (auto* D=GetDebug()) if (D->GetShowDebugShapes()!=bValue) D->ToggleDebugShapes(); }
void UNGDebugSettingsPanel::LayersChanged(bool bValue) { if (auto* D=GetDebug()) if (D->GetShowLayerColors()!=bValue) D->ToggleLayerColors(); }
