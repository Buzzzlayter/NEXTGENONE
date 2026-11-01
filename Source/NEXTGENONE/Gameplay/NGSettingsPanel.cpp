#include "NGSettingsPanel.h"
#include "NextgenonePlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HFSMState.h"
#include "NGDebugState.h"
#include "Input/PlayerInputSubsystem.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "NGConnectionPanel.h"
#include "NGDebugSettingsPanel.h"

void UNGInputPreferences::ApplyAudio(UWorld* World) const
{
	if (World) if (auto Device=World->GetAudioDevice()) Device->SetTransientPrimaryVolume(FMath::Clamp(MasterVolume,0.f,1.f));
}

TSharedRef<SWidget> UNGSettingsPanel::RebuildWidget()
{
	if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Background=WidgetTree->ConstructWidget<UBorder>(); Background->SetBrushColor(FLinearColor(.018f,.025f,.04f,.98f)); Background->SetPadding(FMargin(48,28));
		auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Background->SetContent(Scroll);
		auto* Body=WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Body); WidgetTree->RootWidget=Background;
		auto Text=[&](const FString& Value,int32 Size=18)
		{
			auto* Label=WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(FText::FromString(Value)); Label->SetAutoWrapText(true);
			auto Font=Label->GetFont(); Font.Size=Size; Label->SetFont(Font); Body->AddChildToVerticalBox(Label)->SetPadding(FMargin(0,7)); return Label;
		};
		auto Combo=[&](const TCHAR* Name,const FString& Label,const TArray<FString>& Options)
		{
			Text(Label); auto* Field=WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(),Name);
			for (const auto& Option:Options) Field->AddOption(Option); Body->AddChild(Field); return Field;
		};
		Text(TEXT("НАСТРОЙКИ"),32); Text(TEXT("Изменения сохраняются кнопкой «Применить». Esc / «Назад» отменяет несохранённые правки."),16);
		Text(TEXT("ЭКРАН И КАЧЕСТВО"),23);
		Quality=Combo(TEXT("Quality"),TEXT("Общее качество — влияет на тени, эффекты, текстуры и дальность прорисовки"),{TEXT("Низкое"),TEXT("Среднее"),TEXT("Высокое"),TEXT("Эпическое"),TEXT("Кинематографическое"),TEXT("Пользовательское")});
		WindowMode=Combo(TEXT("WindowMode"),TEXT("Режим экрана"),{TEXT("Полноэкранный"),TEXT("Без рамки"),TEXT("Оконный")});
		Resolution=Combo(TEXT("Resolution"),TEXT("Разрешение экрана"),{});
		FrameLimit=Combo(TEXT("FrameLimit"),TEXT("Ограничение FPS — 0 отключает лимит"),{TEXT("0"),TEXT("30"),TEXT("60"),TEXT("90"),TEXT("120"),TEXT("144"),TEXT("165"),TEXT("240")});
		Text(TEXT("Вертикальная синхронизация — уменьшает разрывы изображения")); VSync=WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),TEXT("VSync")); Body->AddChild(VSync);
		Text(TEXT("ЗВУК"),23); Text(TEXT("Общая громкость — все звуки игры"));
		Volume=WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),TEXT("MasterVolume")); Volume->SetMinValue(0.f); Volume->SetMaxValue(1.f); Body->AddChild(Volume);
		Text(TEXT("УПРАВЛЕНИЕ КАМЕРОЙ"),23); Text(TEXT("Чувствительность обзора: от 0.1 до 3.0 (1.0 — стандарт ALS)"));
		Sensitivity=WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),TEXT("Sensitivity")); Sensitivity->SetMinValue(.1f); Sensitivity->SetMaxValue(3.f); Sensitivity->SetStepSize(.05f); Body->AddChild(Sensitivity);
		Text(TEXT("Инвертировать вертикальный обзор")); Invert=WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),TEXT("InvertY")); Body->AddChild(Invert);
		Text(TEXT("КЛАВИШИ"),23);
		Text(TEXT("W A S D — движение · Мышь — обзор\nSpace — прыжок · Q — деш только в воздухе · V — залезание\nE — взаимодействие / подбор предмета · I — инвентарь\nЛКМ — стрельба / удар кулаком без оружия · ПКМ — прицеливание\n1 — пистолет · 2 — автомат · 3 — дробовик · R — перезарядка · U — убрать оружие\nEsc — настройки / назад · F — тестовая экипировка"));
		Text(TEXT("Дополнительные привязки ALS задаются в DefaultInputMappingContext. Переназначение клавиш в этом экране пока не предусмотрено."),14);
		Text(TEXT("СЕТЬ"),23);
		if (GetOwningPlayer()) if (auto* Network=CreateWidget<UNGConnectionPanel>(GetOwningPlayer(),ConnectionPanelClass ? ConnectionPanelClass.Get() : UNGConnectionPanel::StaticClass())) Body->AddChild(Network);
#if !UE_BUILD_SHIPPING
		Text(TEXT("ДИАГНОСТИКА"),23);
		if (GetOwningPlayer()) if (auto* Debug=CreateWidget<UNGDebugSettingsPanel>(GetOwningPlayer(),DebugPanelClass ? DebugPanelClass.Get() : UNGDebugSettingsPanel::StaticClass())) Body->AddChild(Debug);
#endif
		Status=Text(TEXT(""),16);
		auto* Buttons=WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChild(Buttons);
		auto Button=[&](const FString& Label)
		{
			auto* B=WidgetTree->ConstructWidget<UButton>(); auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Label)); B->SetContent(T); Buttons->AddChild(B); return B;
		};
		Button(TEXT("  Применить  "))->OnClicked.AddDynamic(this,&ThisClass::Apply);
		Button(TEXT("  По умолчанию  "))->OnClicked.AddDynamic(this,&ThisClass::Defaults);
		Button(TEXT("  Назад  "))->OnClicked.AddDynamic(this,&ThisClass::Cancel);
	}
	// WidgetTree is rebuilt on re-entry; recover references from an authored widget too.
	Quality=Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("Quality"))); WindowMode=Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("WindowMode")));
	Resolution=Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("Resolution"))); FrameLimit=Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("FrameLimit")));
	VSync=Cast<UCheckBox>(WidgetTree->FindWidget(TEXT("VSync"))); Invert=Cast<UCheckBox>(WidgetTree->FindWidget(TEXT("InvertY"))); Sensitivity=Cast<USlider>(WidgetTree->FindWidget(TEXT("Sensitivity")));
	Volume=Cast<USlider>(WidgetTree->FindWidget(TEXT("MasterVolume")));
	return Super::RebuildWidget();
}
void UNGSettingsPanel::NativeConstruct() { Super::NativeConstruct(); SetIsFocusable(true); LoadValues(); }
void UNGSettingsPanel::LoadValues()
{
	auto* S=UGameUserSettings::GetGameUserSettings(); if (!S || !Quality || !Resolution || !FrameLimit || !WindowMode || !VSync || !Invert || !Sensitivity) return;
	Resolutions.Reset(); UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions); Resolutions.AddUnique(S->GetScreenResolution());
	Resolutions.Sort([](const FIntPoint& A,const FIntPoint& B){ return A.X==B.X ? A.Y<B.Y : A.X<B.X; });
	Resolution->ClearOptions(); for (auto R:Resolutions) Resolution->AddOption(FString::Printf(TEXT("%d × %d"),R.X,R.Y));
	Resolution->SetSelectedIndex(Resolutions.IndexOfByKey(S->GetScreenResolution())); Quality->SetSelectedIndex(S->GetOverallScalabilityLevel()<0 ? 5 : S->GetOverallScalabilityLevel());
	WindowMode->SetSelectedIndex(static_cast<int32>(S->GetFullscreenMode())); VSync->SetIsChecked(S->IsVSyncEnabled());
	const FString FPS=FString::FromInt(FMath::RoundToInt(S->GetFrameRateLimit())); if (FrameLimit->FindOptionIndex(FPS)==INDEX_NONE) FrameLimit->AddOption(FPS); FrameLimit->SetSelectedOption(FPS);
	const auto* P=GetDefault<UNGInputPreferences>(); Sensitivity->SetValue(P->LookSensitivity); Invert->SetIsChecked(P->bInvertY);
	if (Volume) Volume->SetValue(P->MasterVolume);
}
void UNGSettingsPanel::Apply()
{
	auto* S=UGameUserSettings::GetGameUserSettings(); if (!S || !Quality || !Resolution || !FrameLimit || !WindowMode || !VSync || !Invert || !Sensitivity) return;
	if (Quality->GetSelectedIndex()>=0 && Quality->GetSelectedIndex()<5) S->SetOverallScalabilityLevel(Quality->GetSelectedIndex());
	if (Resolutions.IsValidIndex(Resolution->GetSelectedIndex())) S->SetScreenResolution(Resolutions[Resolution->GetSelectedIndex()]);
	S->SetFullscreenMode(static_cast<EWindowMode::Type>(FMath::Clamp(WindowMode->GetSelectedIndex(),0,2))); S->SetVSyncEnabled(VSync->IsChecked()); S->SetFrameRateLimit(FCString::Atof(*FrameLimit->GetSelectedOption()));
	S->ApplySettings(false); S->ConfirmVideoMode(); S->SaveSettings();
	auto* P=GetMutableDefault<UNGInputPreferences>(); P->LookSensitivity=FMath::Clamp(Sensitivity->GetValue(),.1f,3.f); P->bInvertY=Invert->IsChecked(); P->SaveConfig();
	if (Volume) { P->MasterVolume=FMath::Clamp(Volume->GetValue(),0.f,1.f); P->ApplyAudio(GetWorld()); P->SaveConfig(); }
	if (Status) Status->SetText(FText::FromString(TEXT("Настройки сохранены.")));
}
void UNGSettingsPanel::Defaults()
{
	if (!Quality || !FrameLimit || !VSync || !Sensitivity || !Invert) return;
	Quality->SetSelectedIndex(2); FrameLimit->SetSelectedOption(TEXT("0")); VSync->SetIsChecked(false); Sensitivity->SetValue(1.f); Invert->SetIsChecked(false);
	if (Volume) Volume->SetValue(1.f);
	if (Status) Status->SetText(FText::FromString(TEXT("Стандартное качество и управление выбраны. Нажмите «Применить» для сохранения.")));
}
void UNGSettingsPanel::Cancel() { LoadValues(); OnBack.Broadcast(); }
FReply UNGSettingsPanel::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
	if (Event.GetKey()==EKeys::Escape) { Cancel(); return FReply::Handled(); } return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
UNGSettingsScreenMounter::UNGSettingsScreenMounter() { WidgetClass=UNGSettingsPanel::StaticClass(); bAddToViewportIfNoLayout=true; FallbackZOrder=100; }
bool UNGSettingsScreenMounter::Enter()
{
	if (!Super::Enter()) return false;
	auto* PC=Cast<ANextgenonePlayerController>(GetPlayerController()); auto* Panel=Cast<UNGSettingsPanel>(GetWidget());
	if (!PC || !Panel) { Super::AbortEnter(); return false; }
	PC->NGFireStopForUI(); bPreviousCursor=PC->bShowMouseCursor; PC->bSettingsUIOpen=true; PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); PC->bShowMouseCursor=true; bLocked=true;
	Panel->OnBack.AddUniqueDynamic(this,&ThisClass::Back); Panel->SetUserFocus(PC);
	if (auto* Input=UPlayerInputSubsystem::GetInputSubsystem(this)) Input->SetWidgetToFocus(Panel);
	return true;
}
void UNGSettingsScreenMounter::Back() { NGTriggerUIEvent(GetState(),TEXT("Back")); }
void UNGSettingsScreenMounter::Release()
{
	if (auto* Panel=Cast<UNGSettingsPanel>(GetWidget())) Panel->OnBack.RemoveDynamic(this,&ThisClass::Back);
	if (bLocked) if (auto* PC=Cast<ANextgenonePlayerController>(GetPlayerController())) { PC->bSettingsUIOpen=false; PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); PC->bShowMouseCursor=bPreviousCursor; }
	bLocked=false;
}
void UNGSettingsScreenMounter::Exit(FName Event) { Release(); Super::Exit(Event); }
void UNGSettingsScreenMounter::AbortEnter() { Release(); Super::AbortEnter(); }
