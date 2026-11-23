#include "Online/NGConnectionPanel.h"
#include "Online/NGConnectionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

TSharedRef<SWidget> UNGConnectionPanel::RebuildWidget()
{
	if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Body=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Body;
		const auto Label = [this](const TCHAR* Caption)
		{
			auto* TextWidget = WidgetTree->ConstructWidget<UTextBlock>();
			TextWidget->SetText(FText::FromString(Caption));
			return TextWidget;
		};
		const auto Button = [this, &Label](UHorizontalBox* Row, const TCHAR* Caption)
		{
			auto* WidgetButton = WidgetTree->ConstructWidget<UButton>();
			WidgetButton->SetContent(Label(Caption));
			Row->AddChild(WidgetButton);
			return WidgetButton;
		};
		PartyHeading = Label(TEXT("Группа"));
		Body->AddChild(PartyHeading);
		auto* LoginButtons = WidgetTree->ConstructWidget<UHorizontalBox>();
		Body->AddChild(LoginButtons);
		SteamButton = Button(LoginButtons, TEXT("  Войти через Steam  "));
		SteamButton->OnClicked.AddDynamic(this, &ThisClass::SignInSteam);
		EpicButton = Button(LoginButtons, TEXT("  Войти через Epic Games  "));
		EpicButton->OnClicked.AddDynamic(this, &ThisClass::SignInEpic);
		auto* GroupButtons = WidgetTree->ConstructWidget<UHorizontalBox>();
		Body->AddChild(GroupButtons);
		CreateButton = Button(GroupButtons, TEXT("  Создать группу  "));
		CreateButton->OnClicked.AddDynamic(this, &ThisClass::CreateParty);
		PartyCodeInput = WidgetTree->ConstructWidget<UEditableTextBox>();
		PartyCodeInput->SetHintText(FText::FromString(TEXT("Код группы")));
		GroupButtons->AddChild(PartyCodeInput);
		JoinButton = Button(GroupButtons, TEXT("  Присоединиться  "));
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinParty);
		PartyRoster = WidgetTree->ConstructWidget<UVerticalBox>();
		Body->AddChild(PartyRoster);
		auto* MatchButtons = WidgetTree->ConstructWidget<UHorizontalBox>();
		Body->AddChild(MatchButtons);
		PlayButton = Button(MatchButtons, TEXT("  Запустить миссию  "));
		PlayLabel = CastChecked<UTextBlock>(PlayButton->GetContent());
		PlayButton->OnClicked.AddDynamic(this, &ThisClass::PlayParty);
		LeaveButton = Button(MatchButtons, TEXT("  Выйти из группы  "));
		LeaveButton->OnClicked.AddDynamic(this, &ThisClass::LeaveParty);
		Message=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("ConnectionStatus")); Message->SetAutoWrapText(true); Body->AddChild(Message);

		Body->AddChildToVerticalBox(Label(TEXT("Подключение по адресу")))->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		Address=WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("ServerAddress")); Address->SetHintText(FText::FromString(TEXT("IPv4 / DNS:порт — например 127.0.0.1:7777"))); Body->AddChild(Address);
		auto* DirectButtons=WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChild(DirectButtons);
		Button(DirectButtons, TEXT("  Подключиться  "))->OnClicked.AddDynamic(this,&ThisClass::Connect);
		Button(DirectButtons, TEXT("  Отключиться  "))->OnClicked.AddDynamic(this,&ThisClass::Disconnect);
	}
	Address=Cast<UEditableTextBox>(WidgetTree->FindWidget(TEXT("ServerAddress"))); Message=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ConnectionStatus")));
	return Super::RebuildWidget();
}
void UNGConnectionPanel::NativeConstruct()
{
	Super::NativeConstruct(); Connection=GetGameInstance() ? GetGameInstance()->GetSubsystem<UNGConnectionSubsystem>() : nullptr;
	if (Connection.IsValid())
	{
		Connection->OnStatusChanged.AddUniqueDynamic(this,&ThisClass::StatusChanged);
		PartyChangedHandle = Connection->OnPartyChanged.AddUObject(this, &ThisClass::RefreshParty);
		if (Address) Address->SetText(FText::FromString(Connection->LastAddress));
		StatusChanged(Connection->GetStatus());
	}
}
void UNGConnectionPanel::NativeDestruct()
{
	if (Connection.IsValid())
	{
		Connection->OnStatusChanged.RemoveDynamic(this,&ThisClass::StatusChanged);
		Connection->OnPartyChanged.Remove(PartyChangedHandle);
	}
	PartyChangedHandle.Reset();
	Connection.Reset();
	Super::NativeDestruct();
}
void UNGConnectionPanel::Connect() { if (Connection.IsValid() && Address) Connection->Connect(GetOwningPlayer(),Address->GetText().ToString()); }
void UNGConnectionPanel::Disconnect()
{
	if (!Connection.IsValid()) return;
	if (Connection->IsInParty() && Connection->IsPartyLeader())
	{
		Connection->LeaveParty();
		return;
	}
	Connection->Disconnect();
}
void UNGConnectionPanel::SignInSteam() { if (Connection.IsValid()) Connection->SignIn(GetOwningPlayer(), true); }
void UNGConnectionPanel::SignInEpic() { if (Connection.IsValid()) Connection->SignIn(GetOwningPlayer(), false); }
void UNGConnectionPanel::CreateParty() { if (Connection.IsValid()) Connection->CreateParty(); }
void UNGConnectionPanel::JoinParty() { if (Connection.IsValid() && PartyCodeInput) Connection->JoinParty(PartyCodeInput->GetText().ToString()); }
void UNGConnectionPanel::PlayParty() { if (Connection.IsValid()) Connection->PlayParty(); }
void UNGConnectionPanel::LeaveParty() { if (Connection.IsValid()) Connection->LeaveParty(); }
void UNGConnectionPanel::StatusChanged(FText Text)
{
	if (Message) Message->SetText(Text);
	RefreshParty();
}

void UNGConnectionPanel::RefreshParty()
{
	const auto* Network = Connection.Get();
	if (!Network || !PartyRoster) return;
	const bool bBusy = Network->IsOnlineBusy();
	const bool bSignedIn = Network->IsOnlineSignedIn();
	const bool bInParty = Network->IsInParty();
	const bool bLeader = Network->IsPartyLeader();
	const bool bMatchReady = Network->IsPartyMatchReady();
	const bool bStandalone = GetWorld() && GetWorld()->GetNetMode() == NM_Standalone;
	SteamButton->SetIsEnabled(!bBusy && !bSignedIn && bStandalone);
	EpicButton->SetIsEnabled(!bBusy && !bSignedIn && bStandalone);
	CreateButton->SetIsEnabled(!bBusy && bSignedIn && !bInParty && bStandalone);
	JoinButton->SetIsEnabled(!bBusy && bSignedIn && !bInParty && bStandalone);
	PartyCodeInput->SetIsReadOnly(bInParty);
	PartyCodeInput->SetIsEnabled(bInParty || !bBusy);
	if (bInParty && PartyCodeInput->GetText().ToString() != Network->GetPartyCode())
		PartyCodeInput->SetText(FText::FromString(Network->GetPartyCode()));
	PlayButton->SetVisibility(bInParty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	LeaveButton->SetVisibility(bInParty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	PlayButton->SetIsEnabled(!bBusy && (bLeader ? !bMatchReady : bMatchReady && bStandalone));
	LeaveButton->SetIsEnabled(!bBusy);
	PlayLabel->SetText(FText::FromString(bLeader ? TEXT("  Запустить миссию  ") : TEXT("  Войти в миссию группы  ")));
	PartyHeading->SetText(FText::FromString(bInParty ? TEXT("Код группы: ") + Network->GetPartyCode() : FString(TEXT("Группа"))));
	PartyRoster->ClearChildren();
	for (const FNGPartyMember& Member : Network->GetPartyMembers())
	{
		auto* MemberRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		auto* Badge = WidgetTree->ConstructWidget<UBorder>();
		Badge->SetBrushColor(FLinearColor(0.09f, 0.25f, 0.36f, 1.f));
		Badge->SetPadding(FMargin(8.f, 3.f));
		auto* Initials = WidgetTree->ConstructWidget<UTextBlock>();
		Initials->SetText(FText::FromString(Member.Name.Left(2).ToUpper()));
		Badge->SetContent(Initials);
		MemberRow->AddChild(Badge);
		FString Caption = TEXT("  ") + Member.Name;
		if (Member.bLocal) Caption += TEXT(" · вы");
		if (Member.bLeader) Caption += TEXT(" · лидер");
		auto* NameLabel = WidgetTree->ConstructWidget<UTextBlock>();
		NameLabel->SetText(FText::FromString(Caption));
		MemberRow->AddChild(NameLabel);
		PartyRoster->AddChildToVerticalBox(MemberRow)->SetPadding(FMargin(0.f, 3.f));
	}
}
