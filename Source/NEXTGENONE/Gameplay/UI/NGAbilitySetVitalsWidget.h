#pragma once

#include "Blueprint/UserWidget.h"
#include "NGAbilitySetVitalsWidget.generated.h"

class UAbilitySystemComponent;
class UImage;
class UMaterialInterface;
class APawn;
struct FOnAttributeChangeData;

UCLASS(PrioritizeCategories=("Vitals"))
class NEXTGENONE_API UNGAbilitySetVitalsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, Category="Vitals") TObjectPtr<UMaterialInterface> HealthMaterial;
	UPROPERTY(EditDefaultsOnly, Category="Vitals") TObjectPtr<UMaterialInterface> OuterShieldMaterial;
	UPROPERTY(EditDefaultsOnly, Category="Vitals") TObjectPtr<UMaterialInterface> InnerShieldMaterial;
	UPROPERTY(EditDefaultsOnly, Category="Vitals", meta=(ClampMin="1")) FVector2D BarSize = FVector2D(200.f, 14.f);
	UPROPERTY(EditDefaultsOnly, Category="Vitals", meta=(ClampMin="0")) float BarSpacing = 4.f;

private:
	UFUNCTION() void PawnChanged(APawn* OldPawn, APawn* NewPawn);
	void AttributesChanged(const FOnAttributeChangeData& Change);
	void UnbindAttributes();
	void RefreshBars();

	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	UPROPERTY(Transient) TObjectPtr<UImage> HealthBar;
	UPROPERTY(Transient) TObjectPtr<UImage> OuterShieldBar;
	UPROPERTY(Transient) TObjectPtr<UImage> InnerShieldBar;
};
