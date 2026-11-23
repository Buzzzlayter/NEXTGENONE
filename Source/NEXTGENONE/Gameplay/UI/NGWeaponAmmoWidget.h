#pragma once

#include "Blueprint/UserWidget.h"
#include "NGWeaponAmmoWidget.generated.h"

class APawn;
class APlayerController;
class UTextBlock;
class UNGCombatComponent;

UCLASS(Abstract)
class NEXTGENONE_API UNGWeaponAmmoWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> AmmoText;

	UFUNCTION() void PawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void AmmoChanged(int32 MagazineAmmo);
	void UnbindCombat();
	void RefreshAmmo();

	TWeakObjectPtr<APlayerController> ObservedController;
	TWeakObjectPtr<UNGCombatComponent> ObservedCombat;
};
