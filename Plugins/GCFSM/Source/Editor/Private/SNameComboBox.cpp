#if ENGINE_MAJOR_VERSION ==	4 && ENGINE_MINOR_VERSION < 19

// Class SNameComboBox was not marked with GRAPHEDITOR_API in Unreal 4.18 despite being public
// This has been fixed in Unreal 4.19, but, in order to guarantee backward compatibility without link errors
// the implementation is being copied verbatim here from Engine\Source\Editor\GraphEditor\Private\KismetPins\SNameComboBox.cpp

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SNameComboBox.h"

void SNameComboBox::Construct( const FArguments& InArgs )
{
	SelectionChanged = InArgs._OnSelectionChanged;
	GetTextLabelForItem = InArgs._OnGetNameLabelForItem;

	// Then make widget
	this->ChildSlot
	[
		SAssignNew(NameCombo, SComboBox< TSharedPtr<FName> > )
		.OptionsSource(InArgs._OptionsSource)
		.OnGenerateWidget(this, &SNameComboBox::MakeItemWidget)
		.OnSelectionChanged(this, &SNameComboBox::OnSelectionChanged)
		.OnComboBoxOpening(InArgs._OnComboBoxOpening)
		.InitiallySelectedItem(InArgs._InitiallySelectedItem)
		.ContentPadding(InArgs._ContentPadding)
		[
			SNew(STextBlock)
				.ColorAndOpacity(InArgs._ColorAndOpacity)
				.Text(this, &SNameComboBox::GetSelectedNameLabel)
		]
	];
	SelectedItem = NameCombo->GetSelectedItem();
}

FText SNameComboBox::GetItemNameLabel(TSharedPtr<FName> NameItem) const
{
	if (!NameItem.IsValid())
	{
		return FText::GetEmpty();
	}

	return (GetTextLabelForItem.IsBound())
		? FText::FromString(GetTextLabelForItem.Execute(NameItem))
		: FText::FromName(*NameItem);
}

FText SNameComboBox::GetSelectedNameLabel() const
{
	TSharedPtr<FName> StringItem = NameCombo->GetSelectedItem();
	return GetItemNameLabel(StringItem);
}

TSharedRef<SWidget> SNameComboBox::MakeItemWidget( TSharedPtr<FName> NameItem ) 
{
	check( NameItem.IsValid() );

	return SNew(STextBlock)
		.Text(this, &SNameComboBox::GetItemNameLabel, NameItem);
}

void SNameComboBox::OnSelectionChanged (TSharedPtr<FName> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid())
	{
		SelectedItem = Selection;
	}
	SelectionChanged.ExecuteIfBound(Selection, SelectInfo);
}

void SNameComboBox::SetSelectedItem(TSharedPtr<FName> NewSelection)
{
	NameCombo->SetSelectedItem(NewSelection);
}

void SNameComboBox::RefreshOptions()
{
	NameCombo->RefreshOptions();
}

void SNameComboBox::ClearSelection( )
{
	NameCombo->ClearSelection();
}

#endif