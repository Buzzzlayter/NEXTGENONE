#pragma once

#include "CoreMinimal.h"
#include "Components/NamedSlot.h"
#include "LayoutTemplate/WidgetPlaceholder.h"
#include "DynamicPlaceholder.generated.h"

UCLASS(BlueprintType, meta = (DisplayName = "Dynamic Placeholder"))
class UIMODULE_API UDynamicPlaceholder : public UNamedSlot, public IWidgetPlaceholder
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder")
	FGameplayTag PlaceholderTag;

	virtual FGameplayTag GetID_Implementation() const override;
	virtual bool MountWidget_Implementation(UUserWidget* Widget) override;
	virtual bool DemountWidget_Implementation(UUserWidget* Widget) override;
	virtual bool ShowWidgets_Implementation(TArray<TSubclassOf<UUserWidget>>& Widgets) override;
	virtual bool HideWidgets_Implementation(TArray<TSubclassOf<UUserWidget>>& Widgets) override;
	virtual void ShowAnimation_Implementation() override;
	virtual void SendVisibility_Implementation(bool bCondition, const FGameplayTag& HidePesetTag) override;
	virtual void Dispatch_Implementation(FGameplayTag Event) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	bool MatchesWidgetList(const TArray<TSubclassOf<UUserWidget>>& Widgets) const;
	UUserWidget* GetMountedUserWidget() const;
};
