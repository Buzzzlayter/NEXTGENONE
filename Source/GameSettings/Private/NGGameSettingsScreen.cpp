#include "NGGameSettingsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameSettingAction.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "GameSettingValueScalar.h"
#include "NGGameSettingRegistry.h"
#include "NGKeyboardSetting.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NGSettings"

namespace
{
TSharedRef<SWidget> MakeControl(UGameSetting& Setting)
{
	if (auto* Toggle = Cast<UGameSettingValueDiscreteDynamic_Bool>(&Setting))
	{
		const auto Weak = MakeWeakObjectPtr(Toggle);
		return SNew(SCheckBox)
			.IsChecked_Lambda([Weak]
			{
				return Weak.IsValid() && Weak->GetDiscreteOptionIndex() == 1 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Weak](ECheckBoxState State)
			{
				if (Weak.IsValid()) Weak->SetDiscreteOptionByIndex(State == ECheckBoxState::Checked ? 1 : 0);
			});
	}

	if (auto* Discrete = Cast<UGameSettingValueDiscrete>(&Setting))
	{
		const auto Weak = MakeWeakObjectPtr(Discrete);
		return SNew(SComboButton)
			.IsEnabled_Lambda([Weak] { return Weak.IsValid() && Weak->GetDiscreteOptions().Num() > 1; })
			.OnGetMenuContent_Lambda([Weak]() -> TSharedRef<SWidget>
			{
				FMenuBuilder Menu(true, nullptr);
				if (Weak.IsValid())
				{
					const TArray<FText> Options = Weak->GetDiscreteOptions();
					for (int32 Index = 0; Index < Options.Num(); ++Index)
					{
						Menu.AddMenuEntry(Options[Index], FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([Weak, Index]
						{
							if (Weak.IsValid() && Weak->GetDiscreteOptions().IsValidIndex(Index))
							{
								Weak->SetDiscreteOptionByIndex(Index);
							}
						})));
					}
				}
				return SNew(SBox).MaxDesiredHeight(320.f)[SNew(SScrollBox) + SScrollBox::Slot()[Menu.MakeWidget()]];
			})
			.ButtonContent()
			[
				SNew(STextBlock).Text_Lambda([Weak]
				{
					if (Weak.IsValid())
					{
						const auto Options = Weak->GetDiscreteOptions();
						const int32 Index = Weak->GetDiscreteOptionIndex();
						if (Options.IsValidIndex(Index)) return Options[Index];
					}
					return LOCTEXT("Unavailable", "Недоступно");
				})
			];
	}

	if (auto* Scalar = Cast<UGameSettingValueScalar>(&Setting))
	{
		const auto Weak = MakeWeakObjectPtr(Scalar);
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SSlider)
					.StepSize(static_cast<float>(Scalar->GetNormalizedStepSize()))
					.Value_Lambda([Weak] { return Weak.IsValid() ? static_cast<float>(Weak->GetValueNormalized()) : 0.f; })
					.OnValueChanged_Lambda([Weak](float Value) { if (Weak.IsValid()) Weak->SetValueNormalized(Value); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.f, 0.f).VAlign(VAlign_Center)
			[
				SNew(SBox).MinDesiredWidth(54.f)
				[
					SNew(STextBlock).Text_Lambda([Weak] { return Weak.IsValid() ? Weak->GetFormattedText() : FText::GetEmpty(); })
				]
			];
	}

	if (auto* Keyboard = Cast<UNGKeyboardSetting>(&Setting))
	{
		const auto Weak = MakeWeakObjectPtr(Keyboard);
		return SNew(SInputKeySelector)
			.SelectedKey_Lambda([Weak] { return FInputChord(Weak.IsValid() ? Weak->GetKey() : EKeys::Invalid); })
			.AllowModifierKeys(false)
			.OnKeySelected_Lambda([Weak](const FInputChord& Chord)
			{
				if (Weak.IsValid())
				{
					Weak->SetKey(Chord.Key);
				}
			});
	}

	if (auto* Action = Cast<UGameSettingAction>(&Setting))
	{
		const auto Weak = MakeWeakObjectPtr(Action);
		return SNew(SButton).Text(Action->GetActionText()).OnClicked_Lambda([Weak]
		{
			if (Weak.IsValid()) Weak->ExecuteAction();
			return FReply::Handled();
		});
	}
	return SNullWidget::NullWidget;
}
}

TSharedRef<SWidget> UNGGameSettingsScreen::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(SupplementalContent, SBox)
				[
					SupplementalWidget ? SupplementalWidget->TakeWidget() : SNullWidget::NullWidget
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				Super::RebuildWidget()
			];
	}

	const auto Weak = MakeWeakObjectPtr(this);
	return SNew(SBorder).Padding(20.f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(SupplementalContent, SBox)
				[
					SupplementalWidget ? SupplementalWidget->TakeWidget() : SNullWidget::NullWidget
				]
			]
			+ SScrollBox::Slot()
			[
				SAssignNew(SettingsRows, SVerticalBox)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton).Text(LOCTEXT("Apply", "Применить"))
					.IsEnabled_Lambda([Weak] { return Weak.IsValid() && Weak->GetRegistry() && Weak->GetRegistry()->HasChanges(); })
					.OnClicked_Lambda([Weak] { if (Weak.IsValid()) Weak->Apply(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton).Text(LOCTEXT("Defaults", "По умолчанию"))
					.OnClicked_Lambda([Weak] { if (Weak.IsValid()) Weak->Defaults(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("Back", "Назад"))
					.OnClicked_Lambda([Weak] { if (Weak.IsValid()) Weak->Cancel(); return FReply::Handled(); })
			]
		]
	];
}

void UNGGameSettingsScreen::SetSupplementalContent(UWidget* Content)
{
	SupplementalWidget = Content;
	if (SupplementalContent)
	{
		SupplementalContent->SetContent(Content ? Content->TakeWidget() : SNullWidget::NullWidget);
	}
}

void UNGGameSettingsScreen::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (ULocalPlayer* Player = GetOwningLocalPlayer(); Player && !Registry)
	{
		Registry = NewObject<UNGGameSettingRegistry>(this);
		Registry->Initialize(Player);
	}
	RefreshSettings();
}

void UNGGameSettingsScreen::NativeDestruct()
{
	if (Registry)
	{
		Registry->RevertChanges();
		Registry = nullptr;
	}
	Super::NativeDestruct();
}

void UNGGameSettingsScreen::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SettingsRows.Reset();
	SupplementalContent.Reset();
}

void UNGGameSettingsScreen::RefreshSettings()
{
	if (!SettingsRows)
	{
		return;
	}

	SettingsRows->ClearChildren();
	if (Registry)
	{
		for (UGameSetting* Section : Registry->GetSections())
		{
			AddSetting(*SettingsRows, *Section);
		}
	}
}

void UNGGameSettingsScreen::AddSetting(SVerticalBox& Body, UGameSetting& Setting)
{
	if (!Setting.GetEditState().IsVisible())
	{
		return;
	}
	if (auto* Collection = Cast<UGameSettingCollection>(&Setting))
	{
		const TArray<UGameSetting*> Children = Collection->GetChildSettings();
		if (Children.ContainsByPredicate([](const UGameSetting* Child) { return Child->GetEditState().IsVisible(); }))
		{
			Body.AddSlot().AutoHeight().Padding(0, 14, 0, 6)
			[
				SNew(STextBlock).Text(Setting.GetDisplayName()).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
			];
			for (UGameSetting* Child : Children) AddSetting(Body, *Child);
		}
		return;
	}

	const auto Weak = MakeWeakObjectPtr(&Setting);
	Body.AddSlot().AutoHeight().Padding(0, 3)
	[
		SNew(SHorizontalBox)
			.IsEnabled_Lambda([Weak] { return Weak.IsValid() && Weak->GetEditState().IsEnabled(); })
			.ToolTipText_Lambda([Weak]
			{
				if (Weak.IsValid())
				{
					const auto& Reasons = Weak->GetEditState().GetDisabledReasons();
					return Reasons.IsEmpty() ? FText::GetEmpty() : Reasons[0];
				}
				return FText::GetEmpty();
			})
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(Setting.GetDisplayName()).AutoWrapText(true)
		]
		+ SHorizontalBox::Slot().FillWidth(.65f).Padding(12, 0, 6, 0).VAlign(VAlign_Center)
		[
			MakeControl(Setting)
		]
	];
}

void UNGGameSettingsScreen::Apply()
{
	if (Registry)
	{
		Registry->SaveChanges();
		RefreshSettings();
	}
}

void UNGGameSettingsScreen::Cancel()
{
	if (Registry)
	{
		Registry->RevertChanges();
		RefreshSettings();
	}
	OnBack.Broadcast();
}

void UNGGameSettingsScreen::Defaults()
{
	if (Registry)
	{
		Registry->ResetToDefaults();
		RefreshSettings();
	}
}

FReply UNGGameSettingsScreen::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (!Event.IsRepeat() && Event.GetKey() == EKeys::Escape)
	{
		Cancel();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
