#include "Gameplay/UI/NGAbilitySetVitalsWidget.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FName VitalFillParameter(TEXT("FillAmount"));

	void SetBarFill(UImage* Bar, float Fill)
	{
		if (Bar)
			if (auto* Material = Bar->GetDynamicMaterial())
				Material->SetScalarParameterValue(VitalFillParameter, FMath::Clamp(Fill, 0.f, 1.f));
	}
}

TSharedRef<SWidget> UNGAbilitySetVitalsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
		WidgetTree->RootWidget = Rows;
		const auto AddBar = [&](UMaterialInterface* Material, float TopPadding)
		{
			auto* Bounds = WidgetTree->ConstructWidget<USizeBox>();
			Bounds->SetWidthOverride(BarSize.X);
			Bounds->SetHeightOverride(BarSize.Y);
			auto* Bar = WidgetTree->ConstructWidget<UImage>();
			if (Material) Bar->SetBrushFromMaterial(Material);
			Bounds->AddChild(Bar);
			Rows->AddChildToVerticalBox(Bounds)->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
			return Bar;
		};
		HealthBar = AddBar(HealthMaterial, 0.f);
		OuterShieldBar = AddBar(OuterShieldMaterial, BarSpacing);
		InnerShieldBar = AddBar(InnerShieldMaterial, BarSpacing);
	}
	return Super::RebuildWidget();
}

void UNGAbilitySetVitalsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (auto* Controller = GetOwningPlayer())
	{
		Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::PawnChanged);
		PawnChanged(nullptr, Controller->GetPawn());
	}
}

void UNGAbilitySetVitalsWidget::NativeDestruct()
{
	if (auto* Controller = GetOwningPlayer()) Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::PawnChanged);
	UnbindAttributes();
	Super::NativeDestruct();
}

void UNGAbilitySetVitalsWidget::PawnChanged(APawn*, APawn* NewPawn)
{
	auto* Controller = GetOwningPlayer();
	auto* ASC = NewPawn && Controller
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Controller->GetPlayerState<APlayerState>())
		: nullptr;
	if (ObservedASC.Get() != ASC)
	{
		UnbindAttributes();
		ObservedASC = ASC;
		if (ASC)
			for (const auto& Attribute : {UNGHealthSet::GetHealthAttribute(), UNGHealthSet::GetMaxHealthAttribute(),
				UNGHealthSet::GetShieldAttribute(), UNGHealthSet::GetMaxShieldAttribute()})
				ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::AttributesChanged);
	}
	RefreshBars();
}

void UNGAbilitySetVitalsWidget::UnbindAttributes()
{
	if (auto* ASC = ObservedASC.Get())
		for (const auto& Attribute : {UNGHealthSet::GetHealthAttribute(), UNGHealthSet::GetMaxHealthAttribute(),
			UNGHealthSet::GetShieldAttribute(), UNGHealthSet::GetMaxShieldAttribute()})
			ASC->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
	ObservedASC.Reset();
}

void UNGAbilitySetVitalsWidget::AttributesChanged(const FOnAttributeChangeData&)
{
	RefreshBars();
}

void UNGAbilitySetVitalsWidget::RefreshBars()
{
	const auto* ASC = ObservedASC.Get();
	const auto* HealthSet = ASC ? ASC->GetSet<UNGHealthSet>() : nullptr;
	const float MaxHealth = HealthSet ? HealthSet->MaxHealth.GetCurrentValue() : 0.f;
	const float MaxShield = HealthSet ? HealthSet->MaxShield.GetCurrentValue() : 0.f;
	const float HealthFill = MaxHealth > 0.f ? HealthSet->Health.GetCurrentValue() / MaxHealth : 0.f;
	const float ShieldFill = MaxShield > 0.f ? 2.f * HealthSet->Shield.GetCurrentValue() / MaxShield : 0.f;
	SetBarFill(HealthBar, HealthFill);
	SetBarFill(OuterShieldBar, ShieldFill - 1.f);
	SetBarFill(InnerShieldBar, ShieldFill);
}
