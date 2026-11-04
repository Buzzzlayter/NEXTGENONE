#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NGDebugSettingsPanel.generated.h"
class UCheckBox;
class UTextBlock;
class UALSDebugComponent;
UCLASS(Blueprintable)
class NEXTGENONE_API UNGDebugSettingsPanel : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
private:
	UPROPERTY(Transient) TObjectPtr<UCheckBox> View;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> Traces;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> Shapes;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> Layers;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Info;
	float RefreshDelay=0.f;
	UALSDebugComponent* GetDebug() const;
	void Refresh();
	UFUNCTION() void ViewChanged(bool bValue);
	UFUNCTION() void TracesChanged(bool bValue);
	UFUNCTION() void ShapesChanged(bool bValue);
	UFUNCTION() void LayersChanged(bool bValue);
};
