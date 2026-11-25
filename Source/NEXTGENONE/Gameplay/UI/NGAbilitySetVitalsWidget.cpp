#include "Gameplay/UI/NGAbilitySetVitalsWidget.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "AbilitySystemGlobals.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
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

void UNGAbilitySetVitalsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (bObserveExplicitPawn)
		ObserveAbilitySystem(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ExplicitPawn.Get()));
	else if (auto* Controller = GetOwningPlayer())
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
	if (bObserveExplicitPawn) return;
	auto* Controller = GetOwningPlayer();
	ObserveAbilitySystem(NewPawn && Controller
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Controller->GetPlayerState<APlayerState>())
		: nullptr);
}

void UNGAbilitySetVitalsWidget::ObservePawn(APawn* Pawn)
{
	if (!bObserveExplicitPawn)
	{
		bObserveExplicitPawn = true;
		if (auto* Controller = GetOwningPlayer())
			Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::PawnChanged);
	}
	ExplicitPawn = Pawn;
	auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (ObservedASC.Get() != ASC) ObserveAbilitySystem(ASC);
}

void UNGAbilitySetVitalsWidget::ObserveAbilitySystem(UAbilitySystemComponent* ASC)
{
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
	const float CurrentHealth = HealthSet ? HealthSet->Health.GetCurrentValue() : 0.f;
	const float CurrentShield = HealthSet ? HealthSet->Shield.GetCurrentValue() : 0.f;
	const float MaxHealth = HealthSet ? HealthSet->MaxHealth.GetCurrentValue() : 0.f;
	const float MaxShield = HealthSet ? HealthSet->MaxShield.GetCurrentValue() : 0.f;
	const float HealthFill = MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f;
	const float ShieldFill = MaxShield > 0.f ? 2.f * CurrentShield / MaxShield : 0.f;
	SetBarFill(HealthBar, HealthFill);
	SetBarFill(OuterShieldBar, ShieldFill - 1.f);
	SetBarFill(InnerShieldBar, ShieldFill);
	OnVitalsUpdated(CurrentHealth, MaxHealth, CurrentShield, MaxShield);
}
