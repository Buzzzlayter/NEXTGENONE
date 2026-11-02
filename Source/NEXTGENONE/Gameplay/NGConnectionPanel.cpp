#include "NGConnectionPanel.h"
#include "NGConnectionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
TSharedRef<SWidget> UNGConnectionPanel::RebuildWidget()
{
	if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Body=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Body;
		auto* Title=WidgetTree->ConstructWidget<UTextBlock>(); Title->SetText(FText::FromString(TEXT("ПОДКЛЮЧЕНИЕ К СЕРВЕРУ"))); Body->AddChild(Title);
		Address=WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("ServerAddress")); Address->SetHintText(FText::FromString(TEXT("IPv4 / DNS:порт — например 127.0.0.1:7777"))); Body->AddChild(Address);
		auto* Buttons=WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChild(Buttons);
		auto Button=[&](const TCHAR* Text) { auto* B=WidgetTree->ConstructWidget<UButton>(); auto* L=WidgetTree->ConstructWidget<UTextBlock>(); L->SetText(FText::FromString(Text)); B->SetContent(L); Buttons->AddChild(B); return B; };
		Button(TEXT("  Подключиться  "))->OnClicked.AddDynamic(this,&ThisClass::Connect);
		Button(TEXT("  Отключиться  "))->OnClicked.AddDynamic(this,&ThisClass::Disconnect);
		auto* Steam=Button(TEXT("  Steam — скоро (App ID не задан)  ")); Steam->SetIsEnabled(false);
		Message=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("ConnectionStatus")); Message->SetAutoWrapText(true); Body->AddChild(Message);
	}
	Address=Cast<UEditableTextBox>(WidgetTree->FindWidget(TEXT("ServerAddress"))); Message=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ConnectionStatus")));
	return Super::RebuildWidget();
}
void UNGConnectionPanel::NativeConstruct()
{
	Super::NativeConstruct(); Connection=GetGameInstance() ? GetGameInstance()->GetSubsystem<UNGConnectionSubsystem>() : nullptr;
	if (Connection.IsValid()) { Connection->OnStatusChanged.AddUniqueDynamic(this,&ThisClass::StatusChanged); if (Address) Address->SetText(FText::FromString(Connection->LastAddress)); StatusChanged(Connection->GetStatus()); }
}
void UNGConnectionPanel::NativeDestruct() { if (Connection.IsValid()) Connection->OnStatusChanged.RemoveDynamic(this,&ThisClass::StatusChanged); Connection.Reset(); Super::NativeDestruct(); }
void UNGConnectionPanel::Connect() { if (Connection.IsValid() && Address) Connection->Connect(GetOwningPlayer(),Address->GetText().ToString()); }
void UNGConnectionPanel::Disconnect() { if (Connection.IsValid()) Connection->Disconnect(); }
void UNGConnectionPanel::StatusChanged(FText Text) { if (Message) Message->SetText(Text); }
