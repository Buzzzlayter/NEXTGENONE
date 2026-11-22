#include "Gameplay/UI/NGWeaponAmmoWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Combat/NGCombat.h"

void UNGWeaponAmmoWidget::NativeConstruct()
{
	Super::NativeConstruct();
	APlayerController* Controller = GetOwningPlayer();
	ObservedController = Controller;
	if (Controller)
	{
		Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::PawnChanged);
	}
	PawnChanged(nullptr, Controller ? Controller->GetPawn() : nullptr);
}

void UNGWeaponAmmoWidget::NativeDestruct()
{
	if (APlayerController* Controller = ObservedController.Get())
	{
		Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::PawnChanged);
	}
	ObservedController.Reset();
	UnbindCombat();
	Super::NativeDestruct();
}

void UNGWeaponAmmoWidget::PawnChanged(APawn*, APawn* NewPawn)
{
	UnbindCombat();
	if (const ANextgenoneCharacter* Character = Cast<ANextgenoneCharacter>(NewPawn))
	{
		if (UNGCombatComponent* Combat = Character->Combat.Get())
		{
			ObservedCombat = Combat;
			Combat->OnAmmoChanged.AddUniqueDynamic(this, &ThisClass::AmmoChanged);
		}
	}
	RefreshAmmo();
}

void UNGWeaponAmmoWidget::AmmoChanged(int32)
{
	RefreshAmmo();
}

void UNGWeaponAmmoWidget::UnbindCombat()
{
	if (UNGCombatComponent* Combat = ObservedCombat.Get())
	{
		Combat->OnAmmoChanged.RemoveDynamic(this, &ThisClass::AmmoChanged);
	}
	ObservedCombat.Reset();
}

void UNGWeaponAmmoWidget::RefreshAmmo()
{
	const UNGCombatComponent* Combat = ObservedCombat.Get();
	if (!AmmoText || !Combat || !Combat->GetEquipment().bEquipped)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	AmmoText->SetText(FText::Format(NSLOCTEXT("NGWeaponAmmoWidget", "AmmoCount", "{0} / {1}"),
		FText::AsNumber(Combat->GetMagazineAmmo()), FText::AsNumber(Combat->GetReserveAmmo())));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
