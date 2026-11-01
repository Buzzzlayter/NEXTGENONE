#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NGConnectionPanel.generated.h"
class UEditableTextBox;
class UTextBlock;
class UNGConnectionSubsystem;
UCLASS(Blueprintable)
class NEXTGENONE_API UNGConnectionPanel : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> Address;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Message;
	TWeakObjectPtr<UNGConnectionSubsystem> Connection;
	UFUNCTION() void Connect();
	UFUNCTION() void Disconnect();
	UFUNCTION() void StatusChanged(FText Text);
};
