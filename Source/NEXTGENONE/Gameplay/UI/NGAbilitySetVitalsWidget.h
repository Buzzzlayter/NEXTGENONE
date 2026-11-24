#pragma once

#include "Blueprint/UserWidget.h"
#include "NGAbilitySetVitalsWidget.generated.h"

class UAbilitySystemComponent;
class UImage;
class APawn;
struct FOnAttributeChangeData;

UCLASS(PrioritizeCategories=("Vitals"))
class NEXTGENONE_API UNGAbilitySetVitalsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vitals")
	void ObservePawn(APawn* Pawn);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Vitals")
	void OnVitalsUpdated(float CurrentHealth, float MaxHealth, float CurrentShield, float MaxShield);

private:
	UFUNCTION() void PawnChanged(APawn* OldPawn, APawn* NewPawn);
	void ObserveAbilitySystem(UAbilitySystemComponent* ASC);
	void AttributesChanged(const FOnAttributeChangeData& Change);
	void UnbindAttributes();
	void RefreshBars();

	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	TWeakObjectPtr<APawn> ExplicitPawn;
	bool bObserveExplicitPawn = false;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> HealthBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> OuterShieldBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> InnerShieldBar;
};
